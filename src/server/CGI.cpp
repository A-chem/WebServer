#include "CGI.hpp"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>
#include <signal.h>

static const int kCgiTimeoutSeconds = 10;

static std::string toStringInt(int value) {
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

static std::string getHeaderValue(const std::map<std::string, std::string>& headers, const std::string& key) {
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		if (it->first.size() != key.size())
			continue;
		bool same = true;
		for (size_t i = 0; i < key.size(); ++i) {
			if (std::tolower(static_cast<unsigned char>(it->first[i])) !=
				std::tolower(static_cast<unsigned char>(key[i]))) {
				same = false;
				break;
			}
		}
		if (same)
			return it->second;
	}
	return "";
}

static std::string makeEnvHeaderName(const std::string& key) {
	std::string out = "HTTP_";
	for (size_t i = 0; i < key.size(); ++i) {
		unsigned char ch = static_cast<unsigned char>(key[i]);
		if (std::isalnum(ch))
			out += static_cast<char>(std::toupper(ch));
		else
			out += '_';
	}
	return out;
}

CGI::CGI(const std::string& scriptPath,
	const std::string& executable,
	const Client& client,
	const LocationConfig& location,
	const ServerConfig& server)
	: _script_path(scriptPath),
	_executable(executable),
	_client(client),
	_location(location),
	_server(server) {}

CGI::~CGI() {}

std::map<std::string, std::string> CGI::buildEnvironment() const {
	std::map<std::string, std::string> env;
	std::map<std::string, std::string> headers = _client.getHeader();

	env["GATEWAY_INTERFACE"] = "CGI/1.1";
	env["SERVER_PROTOCOL"] = _client.getVersion().empty() ? "HTTP/1.1" : _client.getVersion();
	env["REQUEST_METHOD"] = _client.getMethod();
	env["SCRIPT_FILENAME"] = _script_path;
	env["SCRIPT_NAME"] = _client.getPath();
	env["QUERY_STRING"] = _client.getQueryString();
	env["DOCUMENT_ROOT"] = _location.root;
	env["REDIRECT_STATUS"] = "200";
	env["SERVER_SOFTWARE"] = "webserv/1.0";

	std::string host = getHeaderValue(headers, "Host");
	std::string serverName = "localhost";
	if (!host.empty()) {
		size_t colon = host.find(':');
		serverName = (colon == std::string::npos) ? host : host.substr(0, colon);
	} else if (!_server.server_names.empty()) {
		serverName = _server.server_names[0];
	}
	env["SERVER_NAME"] = serverName;

	int port = _server.port;
	if (!_server.listen_sockets.empty())
		port = _server.listen_sockets[0].second;
	env["SERVER_PORT"] = toStringInt(port);

	env["REMOTE_ADDR"] = _client.getRemoteAddr();
	env["REMOTE_PORT"] = _client.getRemotePort();

	std::string requestUri = _client.getPath();
	if (!_client.getQueryString().empty())
		requestUri += "?" + _client.getQueryString();
	env["REQUEST_URI"] = requestUri;

	std::string contentType = getHeaderValue(headers, "Content-Type");
	if (!contentType.empty())
		env["CONTENT_TYPE"] = contentType;

	std::string contentLength = getHeaderValue(headers, "Content-Length");
	if (_client.getMethod() == "POST" || _client.getMethod() == "PUT") {
		if (!contentLength.empty())
			env["CONTENT_LENGTH"] = contentLength;
		else
			env["CONTENT_LENGTH"] = toStringInt(static_cast<int>(_client.getBody().size()));
	}

	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		std::string lower = it->first;
		for (size_t i = 0; i < lower.size(); ++i)
			lower[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[i])));
		if (lower == "content-type" || lower == "content-length")
			continue;
		env[makeEnvHeaderName(it->first)] = it->second;
	}

	return env;
}

static void closeIfOpen(int& fd) {
	if (fd != -1) {
		close(fd);
		fd = -1;
	}
}

bool CGI::parseOutput(const std::string& output, CGIResult& result) {
	size_t headersEnd = output.find("\r\n\r\n");
	size_t splitLen = 4;
	if (headersEnd == std::string::npos) {
		headersEnd = output.find("\n\n");
		splitLen = 2;
	}

	std::string headerBlock;
	if (headersEnd != std::string::npos) {
		headerBlock = output.substr(0, headersEnd);
		result.body = output.substr(headersEnd + splitLen);
	} else {
		result.body = output;
		return true;
	}

	std::istringstream hs(headerBlock);
	std::string line;
	while (std::getline(hs, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.erase(line.size() - 1);
		if (line.empty())
			continue;
		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;
		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);
		size_t start = value.find_first_not_of(" \t");
		if (start != std::string::npos)
			value = value.substr(start);
		else
			value.clear();

		if (key == "Status") {
			std::istringstream ss(value);
			int code;
			if (ss >> code)
				result.status_code = code;
		} else {
			result.headers[key] = value;
		}
	}
	return true;
}

bool CGI::execute(CGIResult& result, std::string& error) const {
	int toChild[2] = {-1, -1};
	int fromChild[2] = {-1, -1};
	if (pipe(toChild) == -1 || pipe(fromChild) == -1) {
		error = "pipe() failed";
		closeIfOpen(toChild[0]);
		closeIfOpen(toChild[1]);
		closeIfOpen(fromChild[0]);
		closeIfOpen(fromChild[1]);
		return false;
	}

	std::map<std::string, std::string> envMap = buildEnvironment();
	std::vector<std::string> envStorage;
	envStorage.reserve(envMap.size());
	for (std::map<std::string, std::string>::const_iterator it = envMap.begin(); it != envMap.end(); ++it)
		envStorage.push_back(it->first + "=" + it->second);

	char** envp = new char*[envStorage.size() + 1];
	for (size_t i = 0; i < envStorage.size(); ++i) {
		envp[i] = new char[envStorage[i].size() + 1];
		std::strcpy(envp[i], envStorage[i].c_str());
	}
	envp[envStorage.size()] = NULL;

	pid_t pid = fork();
	if (pid == -1) {
		error = "fork() failed";
		closeIfOpen(toChild[0]);
		closeIfOpen(toChild[1]);
		closeIfOpen(fromChild[0]);
		closeIfOpen(fromChild[1]);
		for (size_t i = 0; i < envStorage.size(); ++i) delete [] envp[i];
		delete [] envp;
		return false;
	}

	if (pid == 0) {
		dup2(toChild[0], STDIN_FILENO);
		dup2(fromChild[1], STDOUT_FILENO);
		closeIfOpen(toChild[0]);
		closeIfOpen(toChild[1]);
		closeIfOpen(fromChild[0]);
		closeIfOpen(fromChild[1]);

		size_t slash = _script_path.rfind('/');
		if (slash != std::string::npos) {
			std::string scriptDir = _script_path.substr(0, slash);
			if (!scriptDir.empty())
				chdir(scriptDir.c_str());
		}

		if (!_executable.empty()) {
			char* const argv[] = {
				const_cast<char*>(_executable.c_str()),
				const_cast<char*>(_script_path.c_str()),
				NULL
			};
			execve(_executable.c_str(), argv, envp);
		} else {
			char* const argv[] = {
				const_cast<char*>(_script_path.c_str()),
				NULL
			};
			execve(_script_path.c_str(), argv, envp);
		}
		const char execFailMsg[] = "webserv: execve failed for CGI script\n";
		write(STDERR_FILENO, execFailMsg, sizeof(execFailMsg) - 1);
		_exit(1);
	}

	closeIfOpen(toChild[0]);
	closeIfOpen(fromChild[1]);
	fcntl(toChild[1], F_SETFL, O_NONBLOCK);
	fcntl(fromChild[0], F_SETFL, O_NONBLOCK);

	std::string body = _client.getBody();
	std::string output;
	size_t writeOffset = 0;
	bool childExited = false;
	bool outputEof = false;
	int status = 0;
	time_t started = time(NULL);

	while (true) {
		if (toChild[1] != -1) {
			if (writeOffset < body.size()) {
				ssize_t wn = write(toChild[1], body.c_str() + writeOffset, body.size() - writeOffset);
				if (wn > 0) {
					writeOffset += static_cast<size_t>(wn);
				} else if (wn == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
					closeIfOpen(toChild[1]);
				}
			}
			if (writeOffset >= body.size())
				closeIfOpen(toChild[1]);
		}

		if (fromChild[0] != -1) {
			char buf[4096];
			while (true) {
				ssize_t rn = read(fromChild[0], buf, sizeof(buf));
				if (rn > 0)
					output.append(buf, rn);
				else if (rn == 0) {
					outputEof = true;
					closeIfOpen(fromChild[0]);
					break;
				} else {
					if (errno != EAGAIN && errno != EWOULDBLOCK)
						closeIfOpen(fromChild[0]);
					break;
				}
			}
		}

		if (!childExited) {
			pid_t waitRes = waitpid(pid, &status, WNOHANG);
			if (waitRes == pid)
				childExited = true;
		}

		if (childExited && (fromChild[0] == -1 || outputEof))
			break;

		if (time(NULL) - started >= kCgiTimeoutSeconds) {
			kill(pid, SIGKILL);
			waitpid(pid, &status, 0);
			closeIfOpen(toChild[1]);
			closeIfOpen(fromChild[0]);
			for (size_t i = 0; i < envStorage.size(); ++i) delete [] envp[i];
			delete [] envp;
			error = "CGI timeout";
			return false;
		}

		struct pollfd fds[2];
		nfds_t nfds = 0;
		if (toChild[1] != -1) {
			fds[nfds].fd = toChild[1];
			fds[nfds].events = POLLOUT;
			fds[nfds].revents = 0;
			++nfds;
		}
		if (fromChild[0] != -1) {
			fds[nfds].fd = fromChild[0];
			fds[nfds].events = POLLIN;
			fds[nfds].revents = 0;
			++nfds;
		}
		if (nfds > 0)
			poll(fds, nfds, 50);
	}

	closeIfOpen(toChild[1]);
	closeIfOpen(fromChild[0]);
	if (!childExited)
		waitpid(pid, &status, 0);

	for (size_t i = 0; i < envStorage.size(); ++i) delete [] envp[i];
	delete [] envp;

	if (WIFSIGNALED(status)) {
		error = "CGI process was terminated by signal";
		return false;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0 && output.empty()) {
		error = "CGI process exited with failure";
		return false;
	}
	return parseOutput(output, result);
}
