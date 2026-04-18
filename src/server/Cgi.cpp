#include "Server.hpp"
#include "Cgi.hpp"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <cctype>
#include <limits.h>

static std::string toUpperHeaderName(const std::string& key) {
	std::string out;
	for (size_t i = 0; i < key.size(); ++i) {
		char c = key[i];
		if (c == '-') out += '_';
		else out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return out;
}

static const int CGI_TIMEOUT_SECONDS = 5;

static std::string getDefaultCgiBin(const std::string& ext) {
	if (ext == ".php") return "/usr/bin/php-cgi";
	if (ext == ".py") return "/usr/bin/python3";
	return "";
}

static bool hasExt(const std::string& path, const std::string& ext) {
	if (ext.empty() || path.size() < ext.size()) return false;
	return path.compare(path.size() - ext.size(), ext.size(), ext) == 0;
}

bool Server::resolveCgiInterpreter(const LocationConfig& loc, const std::string& path, std::string& cgiBin) const {
	if (loc.cgi_pass.empty()) return false;
	if (!loc.cgi_ext.empty()) {
		if (!hasExt(path, loc.cgi_ext)) return false;
		cgiBin = loc.cgi_pass;
		return true;
	}
	if (!loc.cgi_pass.empty() && loc.cgi_pass[0] == '.') {
		if (!hasExt(path, loc.cgi_pass)) return false;
		cgiBin = getDefaultCgiBin(loc.cgi_pass);
		return !cgiBin.empty();
	}
	cgiBin = loc.cgi_pass;
	return true;
}

Cgi::Cgi()
	: _pid(-1), _inFd(-1), _outFd(-1), _bodySent(0), _startTime(0),
	  _bodyDone(false), _outputDone(false), _contentLength(0),
	  _serverName("localhost"), _serverPort("80"),
	  _remoteAddr("127.0.0.1"), _remotePort("0") {}

Cgi::~Cgi() {
	cleanup();
}

void Cgi::setRequestMethod(const std::string& method) { _requestMethod = method; }
void Cgi::setContentType(const std::string& contentType) { _contentType = contentType; }
void Cgi::setContentLength(size_t contentLength) { _contentLength = contentLength; }
void Cgi::setBody(const std::string& body) { _body = body; }
void Cgi::addHeader(const std::string& key, const std::string& value) { _headers[key] = value; }
void Cgi::setServerInfo(const std::string& name, const std::string& port) { _serverName = name; _serverPort = port; }
void Cgi::setClientInfo(const std::string& addr, const std::string& port) { _remoteAddr = addr; _remotePort = port; }

bool Cgi::buildEnv(std::vector<std::string>& envStore, const std::string& absScriptPath) const {
	std::map<std::string, std::string> envMap;
	std::ostringstream lenStr;

	lenStr << _contentLength;
	envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
	envMap["SERVER_PROTOCOL"] = "HTTP/1.1";
	envMap["SERVER_SOFTWARE"] = "Webserv/1.0";
	envMap["REQUEST_METHOD"] = _requestMethod;
	envMap["QUERY_STRING"] = _queryString;
	envMap["SCRIPT_NAME"] = _scriptUri;
	envMap["PATH_INFO"] = _scriptUri;
	envMap["SCRIPT_FILENAME"] = absScriptPath;
	envMap["REQUEST_URI"] = _scriptUri + (_queryString.empty() ? "" : "?" + _queryString);
	envMap["DOCUMENT_ROOT"] = _docRoot;
	envMap["REDIRECT_STATUS"] = "200";
	envMap["AUTH_TYPE"] = "";
	envMap["REMOTE_ADDR"] = _remoteAddr;
	envMap["REMOTE_PORT"] = _remotePort;
	envMap["SERVER_NAME"] = _serverName;
	envMap["SERVER_PORT"] = _serverPort;

	if (_headers.count("Content-Length"))
		envMap["CONTENT_LENGTH"] = _headers.find("Content-Length")->second;
	else
		envMap["CONTENT_LENGTH"] = lenStr.str();
	if (_headers.count("Content-Type"))
		envMap["CONTENT_TYPE"] = _headers.find("Content-Type")->second;
	else
		envMap["CONTENT_TYPE"] = _contentType;
	if (_headers.count("Host"))
		envMap["HTTP_HOST"] = _headers.find("Host")->second;

	for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
		std::string key = toUpperHeaderName(it->first);
		// Body is already de-chunked before CGI execution, so this header must not propagate.
		if (key == "CONTENT_LENGTH" || key == "CONTENT_TYPE" || key == "TRANSFER_ENCODING")
			continue;
		envMap["HTTP_" + key] = it->second;
	}

	for (std::map<std::string, std::string>::const_iterator it = envMap.begin(); it != envMap.end(); ++it)
		envStore.push_back(it->first + "=" + it->second);
	return true;
}

bool Cgi::initialize(const std::string& scriptPath, const std::string& scriptUri,
					const std::string& queryString, const std::string& cgiBin,
					const std::string& docRoot) {
	int inPipe[2];
	int outPipe[2];
	if (pipe(inPipe) == -1) return false;
	if (pipe(outPipe) == -1) {
		close(inPipe[0]);
		close(inPipe[1]);
		return false;
	}

	_scriptPath = scriptPath;
	_scriptUri = scriptUri;
	_queryString = queryString;
	_cgiBin = cgiBin;
	_docRoot = docRoot;
	_bodySent = 0;
	_bodyDone = false;
	_outputDone = false;
	_output.clear();
	_startTime = time(NULL);

	pid_t child = fork();
	if (child < 0) {
		close(inPipe[0]); close(inPipe[1]);
		close(outPipe[0]); close(outPipe[1]);
		return false;
	}

	if (child == 0) {
		char resolvedScript[PATH_MAX];
		std::string absScriptPath = _scriptPath;
		if (realpath(_scriptPath.c_str(), resolvedScript) != NULL)
			absScriptPath = resolvedScript;
		size_t slash = absScriptPath.find_last_of('/');
		std::string scriptDir = (slash == std::string::npos) ? "." : absScriptPath.substr(0, slash);

		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);
		close(inPipe[1]); close(outPipe[0]);
		close(inPipe[0]); close(outPipe[1]);
		if (chdir(scriptDir.c_str()) != 0) _exit(1);

		std::vector<std::string> envStore;
		buildEnv(envStore, absScriptPath);
		std::vector<char*> envp;
		for (size_t i = 0; i < envStore.size(); ++i) {
			char* envDup = strdup(envStore[i].c_str());
			if (!envDup) {
				for (size_t j = 0; j < envp.size(); ++j)
					free(envp[j]);
				_exit(1);
			}
			envp.push_back(envDup);
		}
		envp.push_back(NULL);

		char* arg0 = strdup(_cgiBin.c_str());
		char* arg1 = strdup(absScriptPath.c_str());
		char* argv[3];
		argv[0] = arg0;
		argv[1] = arg1;
		argv[2] = NULL;
		if (!arg0 || !arg1) {
			free(arg0);
			free(arg1);
			for (size_t i = 0; i < envp.size(); ++i)
				free(envp[i]);
			_exit(1);
		}
		execve(_cgiBin.c_str(), argv, &envp[0]);

		free(arg0);
		free(arg1);
		for (size_t i = 0; i < envp.size(); ++i)
			free(envp[i]);
		_exit(1);
	}

	close(inPipe[0]);
	close(outPipe[1]);

	int flags = fcntl(inPipe[1], F_GETFL, 0);
	if (flags == -1 || fcntl(inPipe[1], F_SETFL, flags | O_NONBLOCK) == -1) {
		close(inPipe[1]); close(outPipe[0]);
		kill(child, SIGKILL);
		waitpid(child, NULL, 0);
		return false;
	}
	flags = fcntl(outPipe[0], F_GETFL, 0);
	if (flags == -1 || fcntl(outPipe[0], F_SETFL, flags | O_NONBLOCK) == -1) {
		close(inPipe[1]); close(outPipe[0]);
		kill(child, SIGKILL);
		waitpid(child, NULL, 0);
		return false;
	}

	_pid = child;
	_inFd = inPipe[1];
	_outFd = outPipe[0];
	return true;
}

bool Cgi::sendBody() {
	if (_bodyDone) return true;
	if (_inFd < 0) {
		_bodyDone = true;
		return true;
	}

	if (_bodySent >= _body.size()) {
		close(_inFd);
		_inFd = -1;
		_bodyDone = true;
		return true;
	}

	ssize_t n = write(_inFd, _body.data() + _bodySent, _body.size() - _bodySent);
	if (n > 0) {
		_bodySent += static_cast<size_t>(n);
		if (_bodySent >= _body.size()) {
			close(_inFd);
			_inFd = -1;
			_bodyDone = true;
			return true;
		}
		return false;
	}
	if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
		return false;

	close(_inFd);
	_inFd = -1;
	_bodyDone = true;
	return true;
}

bool Cgi::readOutput() {
	if (_outputDone) return true;
	if (_outFd < 0) {
		_outputDone = true;
		return true;
	}

	char buf[8192];
	while (true) {
		ssize_t n = read(_outFd, buf, sizeof(buf));
		if (n > 0) {
			_output.append(buf, n);
			continue;
		}
		if (n == 0) {
			close(_outFd);
			_outFd = -1;
			if (_pid > 0) {
				int status = 0;
				if (waitpid(_pid, &status, WNOHANG) == _pid)
					_pid = -1;
			}
			_outputDone = true;
			return true;
		}
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return false;
		close(_outFd);
		_outFd = -1;
		_outputDone = true;
		return true;
	}
}

bool Cgi::hasTimedOut() const {
	if (_startTime == 0) return false;
	return (time(NULL) - _startTime) > CGI_TIMEOUT_SECONDS;
}

void Cgi::terminate() {
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		waitpid(_pid, NULL, 0);
		_pid = -1;
	}
}

void Cgi::cleanup() {
	if (_inFd >= 0) {
		close(_inFd);
		_inFd = -1;
	}
	if (_outFd >= 0) {
		close(_outFd);
		_outFd = -1;
	}
	terminate();
}

const std::string& Cgi::getOutput() const {
	return _output;
}

bool Cgi::parseResponse(const std::string& raw, int& code, std::string& reason,
						std::string& contentType, std::string& body,
						std::vector<std::string>& extraHeaders) {
	if (raw.empty()) return false;

	std::string headerPart;
	size_t splitPos = raw.find("\r\n\r\n");
	size_t splitSize = 4;
	if (splitPos == std::string::npos) {
		splitPos = raw.find("\n\n");
		splitSize = 2;
	}
	if (splitPos == std::string::npos)
		body = raw;
	else {
		headerPart = raw.substr(0, splitPos);
		body = raw.substr(splitPos + splitSize);
	}

	code = 200;
	reason = "OK";
	contentType = "text/html";

	if (!headerPart.empty()) {
		std::istringstream ss(headerPart);
		std::string line;
		while (std::getline(ss, line)) {
			if (!line.empty() && line[line.size() - 1] == '\r')
				line.erase(line.size() - 1);
			if (line.empty()) continue;
			size_t colon = line.find(':');
			if (colon == std::string::npos) continue;
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			while (!value.empty() && (value[0] == ' ' || value[0] == '\t'))
				value.erase(0, 1);
			if (key == "Status") {
				std::istringstream st(value);
				st >> code;
				std::getline(st, reason);
				while (!reason.empty() && reason[0] == ' ')
					reason.erase(0, 1);
				if (reason.empty()) reason = "OK";
			} else if (key == "Content-Type") {
				contentType = value;
			} else if (key != "Content-Length" && key != "Connection") {
				extraHeaders.push_back(key + ": " + value);
			}
		}
	}
	return true;
}
