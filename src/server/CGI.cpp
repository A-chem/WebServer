#include "CGI.hpp"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

CGI::CGI() : _pid(-1), _state(CGI_IDLE), _start_time(0), _timeout(30),
	_http_status(200), _is_chunked(false) {
	_pipe_out[0] = -1;
	_pipe_out[1] = -1;
	_pipe_in[0] = -1;
	_pipe_in[1] = -1;
}

CGI::~CGI() {
	_cleanup();
}

void CGI::_setState(CGIState state) {
	if (_state != state) {
		_state = state;
		std::cout << "[CGI] State: " << getStateName() << std::endl;
	}
}

const char* CGI::getStateName() const {
	switch (_state) {
		case CGI_IDLE:          return "IDLE";
		case CGI_FORK:          return "FORK";
		case CGI_READING:       return "READING";
		case CGI_PARSE_HEADER:  return "PARSE_HEADER";
		case CGI_PARSE_BODY:    return "PARSE_BODY";
		case CGI_DECHUNK:       return "DECHUNK";
		case CGI_FINISHED:      return "FINISHED";
		case CGI_TIMEOUT:       return "TIMEOUT";
		case CGI_ERROR:         return "ERROR";
		default:                return "UNKNOWN";
	}
}

size_t CGI::_hexToSize(const std::string& hex) {
	size_t result = 0;
	for (size_t i = 0; i < hex.length(); i++) {
		char c = hex[i];
		result *= 16;
		if (c >= '0' && c <= '9')
			result += (c - '0');
		else if (c >= 'a' && c <= 'f')
			result += (c - 'a' + 10);
		else if (c >= 'A' && c <= 'F')
			result += (c - 'A' + 10);
	}
	return result;
}

std::string CGI::_dechunkBody(const std::string& chunked) {
	std::string result;
	size_t pos = 0;
	
	while (pos < chunked.length()) {
		size_t crlf = chunked.find("\r\n", pos);
		if (crlf == std::string::npos)
			break;
		
		std::string size_line = chunked.substr(pos, crlf - pos);
		
		size_t ext = size_line.find(';');
		if (ext != std::string::npos)
			size_line = size_line.substr(0, ext);
		
		size_t chunk_size = _hexToSize(size_line);
		
		if (chunk_size == 0)
			break;
		
		pos = crlf + 2;
		
		if (pos + chunk_size + 2 > chunked.length())
			break;
		
		result.append(chunked.substr(pos, chunk_size));
		pos += chunk_size + 2;
	}
	
	return result;
}

void CGI::_parseHeadersState() {
    // 1. Kan-qelbo 3la l-separator bin l-headers o l-body (\r\n\r\n aw \n\n)
    size_t sep = _raw_output.find("\r\n\r\n");
    int sep_len = 4;

    if (sep == std::string::npos) {
        sep = _raw_output.find("\n\n");
        sep_len = 2;
    }

    // 2. Ila lqina l-separator
    if (sep != std::string::npos) {
        _headers = _raw_output.substr(0, sep);
        _body = _raw_output.substr(sep + sep_len);
        std::cout << "[CGI-PARSE] Found separator, length: " << sep_len << std::endl;
    } 
    // 3. ILA MALQINA VALO (Hadi hiya l-mouchkila li 3ndek)
    else {
        std::cout << "[CGI-PARSE] WARNING: No header separator found!" << std::endl;
        // Kan-fardo ana l-output kamel houwa l-body o l-headers khawyin (aw default)
        _headers = ""; 
        _body = _raw_output;
    }

    std::cout << "[CGI-PARSE] Headers length: " << _headers.length() << " bytes" << std::endl;
    std::cout << "[CGI-PARSE] Body length: " << _body.length() << " bytes" << std::endl;

    // 4. Parsing dyal l-headers bach njebdo Status o Transfer-Encoding (ila kanu)
    if (!_headers.empty()) {
        std::istringstream hstream(_headers);
        std::string line;

        while (std::getline(hstream, line)) {
            if (line.empty() || line == "\r") continue;

            // Remove \r f l-lekher dyal l-line
            if (!line.empty() && line[line.length() - 1] == '\r')
                line.erase(line.length() - 1);

            size_t colon = line.find(": ");
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string value = line.substr(colon + 2);

                if (key == "Status") {
                    _http_status = std::atoi(value.c_str());
                } else if (key == "Transfer-Encoding" && value == "chunked") {
                    _is_chunked = true;
                }
            }
        }
    }

    // 5. Kan-douzou l-state l-moualiya darori bach ma-ibqach l-CGI "stuck"
    _setState(CGI_PARSE_BODY);
}

void CGI::_parseBodyState() {
	if (_is_chunked) {
		_setState(CGI_DECHUNK);
	} else {
		_setState(CGI_FINISHED);
	}
}

void CGI::_dechunkState() {
	_body = _dechunkBody(_body);
	_setState(CGI_FINISHED);
}

void CGI::setMethod(const std::string& m) { _method = m; }
void CGI::setPath(const std::string& p) { _path = p; }
void CGI::setQuery(const std::string& q) { _query = q; }
void CGI::setBody(const std::string& b) { _body_in = b; }
void CGI::setContentType(const std::string& ct) { _content_type = ct; }
void CGI::setHost(const std::string& h) { _host = h; }

void CGI::_cleanup() {
	if (_pipe_out[0] != -1) close(_pipe_out[0]);
	if (_pipe_out[1] != -1) close(_pipe_out[1]);
	if (_pipe_in[0] != -1) close(_pipe_in[0]);
	if (_pipe_in[1] != -1) close(_pipe_in[1]);
	
	if (_pid > 0) {
		kill(_pid, SIGKILL);
		int status;
		waitpid(_pid, &status, 0);
	}
}

int CGI::execute(const std::string& script, int timeout) {
	_setState(CGI_IDLE);
	_timeout = timeout;
	_start_time = time(NULL);
	
	std::cout << "[CGI] Executing: " << script << std::endl;
	std::cout << "[CGI] Script path: " << _path << std::endl;
	
	if (access(script.c_str(), X_OK) != 0) {
		std::cout << "[CGI] ERROR: Script not executable: " << script << std::endl;
		_setState(CGI_ERROR);
		_http_status = 403;
		return 403;
	}
	
	if (pipe(_pipe_out) < 0 || pipe(_pipe_in) < 0) {
		std::cout << "[CGI] ERROR: Pipe creation failed" << std::endl;
		_cleanup();
		_setState(CGI_ERROR);
		_http_status = 500;
		return 500;
	}
	
	_setState(CGI_FORK);
	_pid = fork();
	
	if (_pid < 0) {
		std::cout << "[CGI] ERROR: Fork failed" << std::endl;
		_cleanup();
		_setState(CGI_ERROR);
		_http_status = 500;
		return 500;
	}
	
	if (_pid == 0) {
		// ===== CHILD PROCESS =====
		close(_pipe_out[0]);
		close(_pipe_in[1]);
		
		dup2(_pipe_out[1], STDOUT_FILENO);
		dup2(_pipe_in[0], STDIN_FILENO);
		
		close(_pipe_out[1]);
		close(_pipe_in[0]);
		
		// Build environment
		std::string env_method = "REQUEST_METHOD=" + _method;
		std::string env_query = "QUERY_STRING=" + _query;
		std::string env_content_type = "CONTENT_TYPE=" + _content_type;
		std::ostringstream content_len_oss;
		content_len_oss << _body_in.size();
		std::string env_content_length = "CONTENT_LENGTH=" + content_len_oss.str();
		std::string env_script = "SCRIPT_FILENAME=" + _path;
		std::string env_gateway = "GATEWAY_INTERFACE=CGI/1.1";
		std::string env_protocol = "SERVER_PROTOCOL=HTTP/1.1";
		std::string env_redirect = "REDIRECT_STATUS=200";
		
		char *env[] = {
			(char *)env_method.c_str(),
			(char *)env_query.c_str(),
			(char *)env_content_type.c_str(),
			(char *)env_content_length.c_str(),
			(char *)env_script.c_str(),
			(char *)env_gateway.c_str(),
			(char *)env_protocol.c_str(),
			(char *)env_redirect.c_str(),
			NULL
		};
		
		// IMPORTANT: For PHP, use the script path, not the interpreter
		// The interpreter is passed as first argument
		const char *args[] = { script.c_str(), _path.c_str(), NULL };
		
		std::cerr << "[CGI-CHILD] Executing: " << script << " " << _path.c_str() << std::endl;
		
		execve(script.c_str(), (char * const *)args, env);
		
		// If execve fails
		std::cerr << "[CGI-CHILD] execve failed: " << strerror(errno) << std::endl;
		exit(127);
	}
	
	// ===== PARENT PROCESS =====
	close(_pipe_out[1]);
	close(_pipe_in[0]);
	
	if (_method == "POST" && !_body_in.empty()) {
		write(_pipe_in[1], _body_in.c_str(), _body_in.size());
	}
	close(_pipe_in[1]);
	
	_setState(CGI_READING);
	
	char buf[8192];
	bool done = false;
	int retries = 0;
	
	while (!done && retries < 3000) { // 30 seconds with 10ms sleep
		if (time(NULL) - _start_time >= _timeout) {
			std::cout << "[CGI] TIMEOUT! Killing process PID=" << _pid << std::endl;
			kill(_pid, SIGKILL);
			int st;
			waitpid(_pid, &st, 0);
			_pid = -1;
			_setState(CGI_TIMEOUT);
			_http_status = 504;
			close(_pipe_out[0]);
			return 504;
		}
		
		int st;
		pid_t res = waitpid(_pid, &st, WNOHANG);
		
		if (res == _pid) {
			std::cout << "[CGI] Process finished. Exit status: " << WEXITSTATUS(st) << std::endl;
			
			// Read all remaining output
			ssize_t n;
			while ((n = read(_pipe_out[0], buf, sizeof(buf))) > 0) {
				_raw_output.append(buf, n);
				std::cout << "[CGI] Read " << n << " bytes (total: " << _raw_output.size() << ")" << std::endl;
			}
			done = true;
			_pid = -1;
		} else if (res < 0) {
			std::cout << "[CGI] Waitpid error" << std::endl;
			done = true;
			_pid = -1;
		} else {
			// Still running - try to read
			ssize_t n = read(_pipe_out[0], buf, sizeof(buf));
			if (n > 0) {
				_raw_output.append(buf, n);
				std::cout << "[CGI] Read " << n << " bytes from pipe (total: " << _raw_output.size() << ")" << std::endl;
			}
			usleep(10000);
			retries++;
		}
	}
	
	close(_pipe_out[0]);
	
	std::cout << "[CGI] Total output: " << _raw_output.size() << " bytes" << std::endl;
	
	if (_raw_output.empty()) {
		std::cout << "[CGI] WARNING: No output from CGI!" << std::endl;
		_http_status = 500;
		_headers = "Content-Type: text/plain";
		_body = "No output from CGI script";
		_setState(CGI_FINISHED);
		return 500;
	}
	
	_setState(CGI_PARSE_HEADER);
	_parseHeadersState();
	_parseBodyState();
	if (_state == CGI_DECHUNK) {
		_dechunkState();
	}
	
	std::cout << "[CGI] Headers: " << _headers.length() << " bytes" << std::endl;
	std::cout << "[CGI] Body: " << _body.length() << " bytes" << std::endl;
	
	return _http_status;
}

CGIState CGI::getState() const { return _state; }
bool CGI::isRunning() const { return _state == CGI_READING || _state == CGI_FORK; }
bool CGI::isFinished() const { return _state == CGI_FINISHED; }
bool CGI::isTimedOut() const { return _state == CGI_TIMEOUT; }
bool CGI::hasError() const { return _state == CGI_ERROR || _state == CGI_TIMEOUT; }
std::string CGI::getBody() const { return _body; }
std::string CGI::getHeaders() const { return _headers; }
int CGI::getStatus() const { return _http_status; }