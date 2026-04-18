#include "Server.hpp"
#include <sstream>
#include <vector>
#include <cctype>
#include <limits.h>
#include <sys/wait.h>

static std::string toUpperHeaderName(const std::string& key) {
	std::string out;
	for (size_t i = 0; i < key.size(); ++i) {
		char c = key[i];
		if (c == '-') out += '_';
		else out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return out;
}

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

bool Server::executeCgi(Client& c, const std::string& scriptPath, const std::string& scriptUri,
	const std::string& query, const std::string& cgiBin, const std::string& docRoot) {
	int inPipe[2];
	int outPipe[2];
	if (pipe(inPipe) == -1 || pipe(outPipe) == -1) return false;

	char resolvedScript[PATH_MAX];
	std::string absScriptPath = scriptPath;
	if (realpath(scriptPath.c_str(), resolvedScript) != NULL)
		absScriptPath = resolvedScript;

	pid_t pid = fork();
	if (pid < 0) return false;
	if (pid == 0) {
		dup2(inPipe[0], STDIN_FILENO);
		dup2(outPipe[1], STDOUT_FILENO);
		close(inPipe[1]);
		close(outPipe[0]);
		close(inPipe[0]);
		close(outPipe[1]);

		size_t slash = absScriptPath.find_last_of('/');
		std::string scriptDir = (slash == std::string::npos) ? "." : absScriptPath.substr(0, slash);
		if (chdir(scriptDir.c_str()) != 0) _exit(1);

		std::map<std::string, std::string> reqHeaders = c.getHeader();
		std::map<std::string, std::string> envMap;
		envMap["GATEWAY_INTERFACE"] = "CGI/1.1";
		envMap["SERVER_PROTOCOL"] = "HTTP/1.1";
		envMap["REQUEST_METHOD"] = c.getMethod();
		envMap["QUERY_STRING"] = query;
		envMap["SCRIPT_NAME"] = scriptUri;
		envMap["PATH_INFO"] = scriptUri;
		envMap["SCRIPT_FILENAME"] = absScriptPath;
		envMap["REQUEST_URI"] = scriptUri + (query.empty() ? "" : "?" + query);
		envMap["SERVER_SOFTWARE"] = "Webserv/1.0";
		envMap["REDIRECT_STATUS"] = "200";
		envMap["DOCUMENT_ROOT"] = docRoot;
		envMap["CONTENT_LENGTH"] = reqHeaders.count("Content-Length") ? reqHeaders["Content-Length"] : "";
		envMap["CONTENT_TYPE"] = reqHeaders.count("Content-Type") ? reqHeaders["Content-Type"] : "";
		if (reqHeaders.count("Host")) envMap["HTTP_HOST"] = reqHeaders["Host"];

		for (std::map<std::string, std::string>::const_iterator it = reqHeaders.begin(); it != reqHeaders.end(); ++it) {
			std::string k = toUpperHeaderName(it->first);
			if (k == "CONTENT_LENGTH" || k == "CONTENT_TYPE") continue;
			envMap["HTTP_" + k] = it->second;
		}

		std::vector<std::string> envStore;
		std::vector<char*> envp;
		for (std::map<std::string, std::string>::const_iterator it = envMap.begin(); it != envMap.end(); ++it)
			envStore.push_back(it->first + "=" + it->second);
		for (size_t i = 0; i < envStore.size(); ++i)
			envp.push_back(const_cast<char*>(envStore[i].c_str()));
		envp.push_back(NULL);

		char* args[3];
		args[0] = const_cast<char*>(cgiBin.c_str());
		args[1] = const_cast<char*>(absScriptPath.c_str());
		args[2] = NULL;
		execve(cgiBin.c_str(), args, &envp[0]);
		_exit(1);
	}

	close(inPipe[0]);
	close(outPipe[1]);
	const std::string body = c.getBody();
	size_t sent = 0;
	while (sent < body.size()) {
		ssize_t n = write(inPipe[1], body.data() + sent, body.size() - sent);
		if (n <= 0) break;
		sent += static_cast<size_t>(n);
	}
	close(inPipe[1]);

	std::string raw;
	char buf[8192];
	while (true) {
		ssize_t n = read(outPipe[0], buf, sizeof(buf));
		if (n <= 0) break;
		raw.append(buf, n);
	}
	close(outPipe[0]);

	int status = 0;
	waitpid(pid, &status, 0);
	std::cerr << "[CGI DEBUG] Output size: " << raw.size() << " bytes" << std::endl;
	std::cerr << "[CGI DEBUG] Output preview (first 200 chars):\n"
	          << raw.substr(0, std::min(raw.size(), static_cast<size_t>(200))) << std::endl;
	if (raw.empty() && !WIFEXITED(status)) return false;

	std::string headerPart;
	std::string bodyPart;
	size_t sep = raw.find("\r\n\r\n");
	size_t sepLen = 4;
	if (sep == std::string::npos) {
		sep = raw.find("\n\n");
		sepLen = 2;
	}
	if (sep == std::string::npos) {
		std::cerr << "[CGI DEBUG] parse failed: missing header/body separator. Full raw output:\n"
		          << raw << std::endl;
		bodyPart = raw;
	}
	else {
		headerPart = raw.substr(0, sep);
		bodyPart = raw.substr(sep + sepLen);
	}

	int code = 200;
	std::string reason = "OK";
	std::string contentType = "text/html";
	std::vector<std::string> passthroughHeaders;
	bool hasContentType = false;

	if (!headerPart.empty()) {
		std::istringstream ss(headerPart);
		std::string line;
		while (std::getline(ss, line)) {
			if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
			if (line.empty()) continue;
			size_t colon = line.find(':');
			if (colon == std::string::npos) continue;
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 1);
			while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) value.erase(0, 1);
			if (key == "Status") {
				std::istringstream st(value);
				st >> code;
				std::getline(st, reason);
				while (!reason.empty() && reason[0] == ' ') reason.erase(0, 1);
				if (reason.empty()) reason = "OK";
			} else if (key == "Content-Type") {
				contentType = value;
				hasContentType = true;
			} else if (key == "Content-Length" || key == "Connection") {
				continue;
			} else {
				passthroughHeaders.push_back(key + ": " + value);
			}
		}
	}
	std::cerr << "[CGI DEBUG] Parsed response: code=" << code
	          << " reason=\"" << reason << "\""
	          << " content_type=\"" << contentType << "\""
	          << " body_size=" << bodyPart.size() << std::endl;
	if (!hasContentType) {
		std::cerr << "[CGI DEBUG] Warning: CGI output did not include Content-Type header" << std::endl;
	}

	std::ostringstream oss;
	oss << "HTTP/1.1 " << code << " " << reason << "\r\n";
	oss << "Server: Webserv/1.0\r\n";
	oss << "Content-Type: " << contentType << "\r\n";
	oss << "Content-Length: " << bodyPart.size() << "\r\n";
	for (size_t i = 0; i < passthroughHeaders.size(); ++i)
		oss << passthroughHeaders[i] << "\r\n";
	oss << "Connection: " << (c.isKeepAlive() ? "keep-alive" : "close") << "\r\n\r\n";
	oss << bodyPart;
	std::cerr << "[CGI DEBUG] Final HTTP response to client:\n" << oss.str() << std::endl;
	c.sendBuf() = oss.str();
	c.setFileSize(c.sendBuf().size());
	return true;
}
