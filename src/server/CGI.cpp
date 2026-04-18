#include "CGI.hpp"
#include "Server.hpp"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

static std::string toUpperHeaderName(std::string s) {
for (size_t i = 0; i < s.size(); ++i) {
if (s[i] >= 'a' && s[i] <= 'z') s[i] -= 32;
else if (s[i] == '-') s[i] = '_';
}
return s;
}

static std::string trim(const std::string& s) {
size_t a = s.find_first_not_of(" \t\r\n");
size_t b = s.find_last_not_of(" \t\r\n");
if (a == std::string::npos) return "";
return s.substr(a, b - a + 1);
}

static std::string dirnameOf(const std::string& path) {
size_t slash = path.rfind('/');
if (slash == std::string::npos) return ".";
if (slash == 0) return "/";
return path.substr(0, slash);
}

static bool childExitedWithError(int status, bool hasNoOutput) {
	return (WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) != 0 && hasNoOutput));
}

static std::string absolutePath(const std::string& path) {
	if (!path.empty() && path[0] == '/') return path;
	char cwd[4096];
	if (!getcwd(cwd, sizeof(cwd))) return path;
	std::string out = cwd;
	if (out[out.size() - 1] != '/') out += "/";
	out += path;
	return out;
}

CGI::CGI()
: _state(CGI_INIT), _pid(-1), _writeOffset(0), _statusCode(200), _statusText("OK") {
_inPipe[0] = -1;
_inPipe[1] = -1;
_outPipe[0] = -1;
_outPipe[1] = -1;
}

CGI::~CGI() {
cleanup(false);
}

void CGI::closeFd(int& fd) {
if (fd >= 0) {
close(fd);
fd = -1;
}
}

void CGI::cleanup(bool reap) {
_state = CGI_CLEANUP;
closeFd(_inPipe[0]);
closeFd(_inPipe[1]);
closeFd(_outPipe[0]);
closeFd(_outPipe[1]);
if (reap && _pid > 0) {
int status;
waitpid(_pid, &status, 0);
}
_pid = -1;
}

bool CGI::setNonBlocking(int fd) {
int flags = fcntl(fd, F_GETFL, 0);
if (flags < 0) return false;
return (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);
}

bool CGI::writeBodyToChild() {
_state = CGI_WRITE;
if (_inPipe[1] < 0) return true;

while (_writeOffset < _body.size()) {
struct pollfd pfd;
pfd.fd = _inPipe[1];
pfd.events = POLLOUT;
pfd.revents = 0;
int pr = poll(&pfd, 1, 100);
if (pr <= 0) return false;
if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) return false;

ssize_t n = write(_inPipe[1], _body.c_str() + _writeOffset, _body.size() - _writeOffset);
if (n > 0) _writeOffset += static_cast<size_t>(n);
else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) return false;
}

closeFd(_inPipe[1]);
return true;
}

bool CGI::readChildOutput() {
_state = CGI_READ;
char buffer[8192];
bool outClosed = false;
bool childExited = false;
int childStatus = 0;

while (!outClosed || !childExited) {
struct pollfd pfd;
pfd.fd = _outPipe[0];
pfd.events = POLLIN | POLLHUP;
pfd.revents = 0;
int pr = poll(&pfd, 1, 100);
if (pr < 0) return false;

if (pr > 0 && (pfd.revents & POLLIN)) {
ssize_t n = read(_outPipe[0], buffer, sizeof(buffer));
if (n > 0) _output.append(buffer, n);
else if (n == 0) outClosed = true;
else if (errno != EAGAIN && errno != EWOULDBLOCK) return false;
}
if (pr > 0 && (pfd.revents & (POLLHUP | POLLERR | POLLNVAL))) outClosed = true;

		if (!childExited && _pid > 0) {
			pid_t w = waitpid(_pid, &childStatus, WNOHANG);
			if (w == _pid) {
				childExited = true;
				if (childExitedWithError(childStatus, _output.empty())) return false;
			}
		}

		if (outClosed && !childExited && _pid > 0) {
			waitpid(_pid, &childStatus, 0);
			childExited = true;
			if (childExitedWithError(childStatus, _output.empty())) return false;
		}
}
return true;
}

void CGI::parseOutput() {
_headers.clear();
_responseBody.clear();
_statusCode = 200;
_statusText = "OK";

if (_output.empty()) return;

size_t split = _output.find("\r\n\r\n");
size_t delimLen = 4;
if (split == std::string::npos) {
split = _output.find("\n\n");
delimLen = 2;
}
if (split == std::string::npos) {
_responseBody = _output;
return;
}

std::string rawHeaders = _output.substr(0, split);
_responseBody = _output.substr(split + delimLen);

std::istringstream hs(rawHeaders);
std::string line;
while (std::getline(hs, line)) {
if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
size_t colon = line.find(':');
if (colon == std::string::npos) continue;
std::string key = trim(line.substr(0, colon));
std::string value = trim(line.substr(colon + 1));
if (key.empty()) continue;
if (key == "Status") {
std::istringstream ss(value);
ss >> _statusCode;
std::getline(ss, _statusText);
_statusText = trim(_statusText);
if (_statusText.empty()) _statusText = "OK";
} else {
_headers[key] = value;
}
}
}

std::string CGI::buildResponse(const Client& c) const {
if (_output.empty()) {
return "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
}

std::ostringstream oss;
oss << "HTTP/1.1 " << _statusCode << " " << _statusText << "\r\n";
for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it)
oss << it->first << ": " << it->second << "\r\n";
if (_headers.find("Content-Type") == _headers.end())
oss << "Content-Type: text/html\r\n";
if (_headers.find("Content-Length") == _headers.end())
oss << "Content-Length: " << _responseBody.size() << "\r\n";
oss << "Connection: " << (c.isKeepAlive() ? "keep-alive" : "close") << "\r\n\r\n";
oss << _responseBody;
return oss.str();
}

bool CGI::execute(const Client& c,
const std::string& scriptName,
const std::string& scriptFilename,
const std::string& pathInfo,
const std::string& executable,
std::string& httpResponse,
std::string& errorMessage) {
	_body = c.getBody();
	_output.clear();
	_writeOffset = 0;
	_state = CGI_INIT;
	std::string absScript = absolutePath(scriptFilename);

if (pipe(_inPipe) != 0 || pipe(_outPipe) != 0) {
errorMessage = "CGI pipe failed";
cleanup(false);
_state = CGI_ERROR;
return false;
}
setNonBlocking(_inPipe[1]);
setNonBlocking(_outPipe[0]);

_state = CGI_FORK;
_pid = fork();
if (_pid < 0) {
errorMessage = "CGI fork failed";
cleanup(false);
_state = CGI_ERROR;
return false;
}

if (_pid == 0) {
_state = CGI_EXEC;
dup2(_inPipe[0], STDIN_FILENO);
dup2(_outPipe[1], STDOUT_FILENO);
closeFd(_inPipe[0]);
closeFd(_inPipe[1]);
closeFd(_outPipe[0]);
closeFd(_outPipe[1]);
		if (chdir(dirnameOf(absScript).c_str()) != 0)
			_exit(126);

std::vector<std::string> envs;
envs.push_back("GATEWAY_INTERFACE=CGI/1.1");
envs.push_back("REQUEST_METHOD=" + c.getMethod());
envs.push_back("QUERY_STRING=" + c.getQuery());
envs.push_back("SCRIPT_NAME=" + scriptName);
		envs.push_back("SCRIPT_FILENAME=" + absScript);
envs.push_back("PATH_INFO=" + pathInfo);
envs.push_back("SERVER_PROTOCOL=" + c.getVersion());
envs.push_back("REMOTE_ADDR=" + c.getRemoteAddr());
envs.push_back("REMOTE_PORT=" + c.getRemotePort());
std::ostringstream cl;
cl << c.getBody().size();
envs.push_back("CONTENT_LENGTH=" + cl.str());

std::map<std::string, std::string> hdrs = c.getHeader();
for (std::map<std::string, std::string>::iterator it = hdrs.begin(); it != hdrs.end(); ++it) {
std::string key = toUpperHeaderName(it->first);
envs.push_back("HTTP_" + key + "=" + it->second);
if (key == "CONTENT_TYPE")
envs.push_back("CONTENT_TYPE=" + it->second);
}

std::vector<char*> envp;
for (size_t i = 0; i < envs.size(); ++i) envp.push_back(const_cast<char*>(envs[i].c_str()));
envp.push_back(NULL);

std::vector<std::string> avs;
if (!executable.empty()) {
avs.push_back(executable);
			avs.push_back(absScript);
		} else {
			avs.push_back(absScript);
		}
std::vector<char*> argv;
for (size_t i = 0; i < avs.size(); ++i) argv.push_back(const_cast<char*>(avs[i].c_str()));
argv.push_back(NULL);

		execve((!executable.empty() ? executable.c_str() : absScript.c_str()), &argv[0], &envp[0]);
_exit(1);
}

closeFd(_inPipe[0]);
closeFd(_outPipe[1]);

if (!writeBodyToChild() || !readChildOutput()) {
errorMessage = "CGI I/O failure";
cleanup(true);
_state = CGI_ERROR;
return false;
}

parseOutput();
httpResponse = buildResponse(c);
cleanup(true);
_state = CGI_DONE;
return true;
}
