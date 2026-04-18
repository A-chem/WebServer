#include "Cgi.hpp"
#include "Server.hpp"

#include <sstream>
#include <cerrno>
#include <cstring>
#include <cstdlib>

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static std::string toLower(const std::string& s) {
    std::string out = s;
    for (size_t i = 0; i < out.size(); ++i) {
        if (out[i] >= 'A' && out[i] <= 'Z')
            out[i] = static_cast<char>(out[i] - 'A' + 'a');
    }
    return out;
}

static std::string getHeaderValue(const std::map<std::string, std::string>& headers,
                                  const std::string& key) {
    std::string target = toLower(key);
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        if (toLower(it->first) == target)
            return it->second;
    }
    return "";
}

static std::string headerToEnv(const std::string& key) {
    std::string out = "HTTP_";
    for (size_t i = 0; i < key.size(); ++i) {
        char c = key[i];
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) out += c;
        else if (c == '-') out += '_';
        else out += '_';
    }
    return out;
}

CgiHandler::CgiHandler() : _pid(-1) {
    _stdin_pipe[0] = _stdin_pipe[1] = -1;
    _stdout_pipe[0] = _stdout_pipe[1] = -1;
}

CgiHandler::~CgiHandler() {
    if (_stdin_pipe[0] != -1) close(_stdin_pipe[0]);
    if (_stdin_pipe[1] != -1) close(_stdin_pipe[1]);
    if (_stdout_pipe[0] != -1) close(_stdout_pipe[0]);
    if (_stdout_pipe[1] != -1) close(_stdout_pipe[1]);
}

bool CgiHandler::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}

std::string CgiHandler::decodeChunked(const std::string& chunked, bool& ok) const {
    ok = false;
    std::string decoded;
    size_t pos = 0;

    while (true) {
        size_t line_end = chunked.find("\r\n", pos);
        if (line_end == std::string::npos) return "";

        std::string size_str = chunked.substr(pos, line_end - pos);
        size_t semi = size_str.find(';');
        if (semi != std::string::npos)
            size_str = size_str.substr(0, semi);
        if (size_str.empty()) return "";

        std::istringstream iss(size_str);
        size_t chunk_size = 0;
        iss >> std::hex >> chunk_size;
        if (iss.fail()) return "";

        pos = line_end + 2;

        if (chunk_size == 0) {
            if (chunked.size() >= pos + 2 && chunked.compare(pos, 2, "\r\n") == 0) {
                ok = true;
                return decoded;
            }
            size_t trailer_end = chunked.find("\r\n\r\n", pos);
            if (trailer_end == std::string::npos) return "";
            ok = true;
            return decoded;
        }

        if (chunked.size() < pos + chunk_size + 2) return "";
        decoded.append(chunked, pos, chunk_size);
        pos += chunk_size;

        if (chunked.compare(pos, 2, "\r\n") != 0) return "";
        pos += 2;
    }
}

bool CgiHandler::buildEnv(Client& client,
                          const ServerConfig& srv,
                          const std::string& request_uri,
                          const std::string& script_path,
                          std::vector<std::string>& env_storage,
                          std::vector<char*>& envp,
                          const std::string& body_for_cgi) {
    std::string script_name = request_uri;
    std::string query;
    size_t q = request_uri.find('?');
    if (q != std::string::npos) {
        script_name = request_uri.substr(0, q);
        query = request_uri.substr(q + 1);
    }

    const std::map<std::string, std::string> headers = client.getHeader();
    std::string content_type = getHeaderValue(headers, "Content-Type");

    std::string remote_addr = "127.0.0.1";
    std::string remote_port = "0";

    struct sockaddr_storage peer;
    socklen_t peer_len = sizeof(peer);
    if (getpeername(client.getFd(), reinterpret_cast<struct sockaddr*>(&peer), &peer_len) == 0) {
        if (peer.ss_family == AF_INET) {
            struct sockaddr_in* in = reinterpret_cast<struct sockaddr_in*>(&peer);
            char ip[INET_ADDRSTRLEN];
            if (inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip)))
                remote_addr = ip;
            std::ostringstream p;
            p << ntohs(in->sin_port);
            remote_port = p.str();
        }
    }

    std::string server_port = "80";
    struct sockaddr_storage local;
    socklen_t local_len = sizeof(local);
    if (getsockname(client.getFd(), reinterpret_cast<struct sockaddr*>(&local), &local_len) == 0) {
        if (local.ss_family == AF_INET) {
            struct sockaddr_in* in = reinterpret_cast<struct sockaddr_in*>(&local);
            std::ostringstream p;
            p << ntohs(in->sin_port);
            server_port = p.str();
        }
    }

    std::string server_name = "localhost";
    if (!srv.server_names.empty())
        server_name = srv.server_names[0];

    std::ostringstream content_len_ss;
    content_len_ss << body_for_cgi.size();
    std::string server_protocol = client.getVersion().empty() ? "HTTP/1.1" : client.getVersion();

    env_storage.push_back("REQUEST_METHOD=" + client.getMethod());
    env_storage.push_back("QUERY_STRING=" + query);
    env_storage.push_back("CONTENT_LENGTH=" + content_len_ss.str());
    env_storage.push_back("CONTENT_TYPE=" + content_type);
    env_storage.push_back("SCRIPT_NAME=" + script_name);
    env_storage.push_back("SCRIPT_FILENAME=" + script_path);
    env_storage.push_back("PATH_INFO=" + script_name);
    env_storage.push_back("SERVER_PROTOCOL=" + server_protocol);
    env_storage.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env_storage.push_back("REMOTE_ADDR=" + remote_addr);
    env_storage.push_back("REMOTE_PORT=" + remote_port);
    env_storage.push_back("SERVER_NAME=" + server_name);
    env_storage.push_back("SERVER_PORT=" + server_port);

    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        env_storage.push_back(headerToEnv(it->first) + "=" + it->second);
    }

    envp.clear();
    for (size_t i = 0; i < env_storage.size(); ++i)
        envp.push_back(const_cast<char*>(env_storage[i].c_str()));
    envp.push_back(NULL);
    return true;
}

bool CgiHandler::parseCgiResponse() {
    if (_cgi_output.empty()) return false;

    if (_cgi_output.find("HTTP/") == 0) {
        _response = _cgi_output;
        return true;
    }

    size_t split = _cgi_output.find("\r\n\r\n");
    size_t delim = 4;
    if (split == std::string::npos) {
        split = _cgi_output.find("\n\n");
        delim = 2;
    }

    if (split == std::string::npos) {
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: " << _cgi_output.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << _cgi_output;
        _response = oss.str();
        return true;
    }

    std::string headers_block = _cgi_output.substr(0, split);
    std::string body = _cgi_output.substr(split + delim);

    int status_code = 200;
    std::string status_text = "OK";
    std::map<std::string, std::string> headers;

    std::istringstream ss(headers_block);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue;

        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos)
            value = value.substr(start);
        else
            value.clear();

        if (toLower(key) == "status") {
            std::istringstream st(value);
            st >> status_code;
            std::string reason;
            std::getline(st, reason);
            if (!reason.empty() && reason[0] == ' ')
                reason.erase(0, 1);
            if (!reason.empty())
                status_text = reason;
        } else {
            headers[key] = value;
        }
    }

    bool has_content_type = false;
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";

    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
        std::string lower_key = toLower(it->first);
        if (lower_key == "content-length" || lower_key == "connection")
            continue;
        if (lower_key == "content-type")
            has_content_type = true;
        oss << it->first << ": " << it->second << "\r\n";
    }

    if (!has_content_type)
        oss << "Content-Type: text/plain\r\n";

    oss << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n\r\n"
        << body;

    _response = oss.str();
    return true;
}

bool CgiHandler::execute(Client& client,
                         const ServerConfig& srv,
                         const std::string& uri_path,
                         const std::string& script_path,
                         const std::string& interpreter) {
    _cgi_output.clear();
    _response.clear();

    std::string body_for_cgi = client.getBody();
    std::map<std::string, std::string> headers = client.getHeader();
    std::string transfer_encoding = toLower(getHeaderValue(headers, "Transfer-Encoding"));
    if (transfer_encoding.find("chunked") != std::string::npos) {
        bool ok = false;
        body_for_cgi = decodeChunked(body_for_cgi, ok);
        if (!ok) return false;
    }

    std::vector<std::string> env_storage;
    std::vector<char*> envp;
    if (!buildEnv(client, srv, uri_path, script_path, env_storage, envp, body_for_cgi))
        return false;

    if (pipe(_stdin_pipe) == -1) return false;
    if (pipe(_stdout_pipe) == -1) {
        close(_stdin_pipe[0]);
        close(_stdin_pipe[1]);
        _stdin_pipe[0] = _stdin_pipe[1] = -1;
        return false;
    }

    _pid = fork();
    if (_pid < 0) {
        close(_stdin_pipe[0]); close(_stdin_pipe[1]);
        close(_stdout_pipe[0]); close(_stdout_pipe[1]);
        _stdin_pipe[0] = _stdin_pipe[1] = -1;
        _stdout_pipe[0] = _stdout_pipe[1] = -1;
        return false;
    }

    if (_pid == 0) {
        dup2(_stdin_pipe[0], STDIN_FILENO);
        dup2(_stdout_pipe[1], STDOUT_FILENO);

        close(_stdin_pipe[0]);
        close(_stdin_pipe[1]);
        close(_stdout_pipe[0]);
        close(_stdout_pipe[1]);

        size_t slash = script_path.rfind('/');
        if (slash != std::string::npos) {
            std::string dir = script_path.substr(0, slash);
            if (!dir.empty() && chdir(dir.c_str()) != 0)
                _exit(127);
        }

        std::string script_arg = script_path;
        if (!script_arg.empty() && script_arg[0] != '/') {
            size_t sep = script_arg.rfind('/');
            if (sep != std::string::npos)
                script_arg = script_arg.substr(sep + 1);
        }

        char* argv[3];
        argv[0] = const_cast<char*>(interpreter.c_str());
        argv[1] = const_cast<char*>(script_arg.c_str());
        argv[2] = NULL;

        execve(interpreter.c_str(), argv, &envp[0]);
        _exit(127);
    }

    close(_stdin_pipe[0]);
    _stdin_pipe[0] = -1;
    close(_stdout_pipe[1]);
    _stdout_pipe[1] = -1;

    if (!setNonBlocking(_stdin_pipe[1]) || !setNonBlocking(_stdout_pipe[0])) {
        kill(_pid, SIGKILL);
        waitpid(_pid, NULL, 0);
        close(_stdin_pipe[1]); _stdin_pipe[1] = -1;
        close(_stdout_pipe[0]); _stdout_pipe[0] = -1;
        return false;
    }

    size_t written = 0;
    bool stdin_open = true;
    bool stdout_open = true;

    if (body_for_cgi.empty()) {
        close(_stdin_pipe[1]);
        _stdin_pipe[1] = -1;
        stdin_open = false;
    }

    while (stdout_open || stdin_open) {
        fd_set readfds;
        fd_set writefds;
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);

        int maxfd = -1;
        if (stdout_open) {
            FD_SET(_stdout_pipe[0], &readfds);
            if (_stdout_pipe[0] > maxfd) maxfd = _stdout_pipe[0];
        }
        if (stdin_open && written < body_for_cgi.size()) {
            FD_SET(_stdin_pipe[1], &writefds);
            if (_stdin_pipe[1] > maxfd) maxfd = _stdin_pipe[1];
        }

        struct timeval tv;
        tv.tv_sec = 10;
        tv.tv_usec = 0;

        int sel = select(maxfd + 1, &readfds, &writefds, NULL, &tv);
        if (sel == 0) {
            kill(_pid, SIGKILL);
            waitpid(_pid, NULL, 0);
            if (_stdin_pipe[1] != -1) { close(_stdin_pipe[1]); _stdin_pipe[1] = -1; }
            if (_stdout_pipe[0] != -1) { close(_stdout_pipe[0]); _stdout_pipe[0] = -1; }
            return false;
        }
        if (sel < 0) {
            if (errno == EINTR) continue;
            kill(_pid, SIGKILL);
            waitpid(_pid, NULL, 0);
            if (_stdin_pipe[1] != -1) { close(_stdin_pipe[1]); _stdin_pipe[1] = -1; }
            if (_stdout_pipe[0] != -1) { close(_stdout_pipe[0]); _stdout_pipe[0] = -1; }
            return false;
        }

        if (stdin_open && FD_ISSET(_stdin_pipe[1], &writefds) && written < body_for_cgi.size()) {
            ssize_t n = write(_stdin_pipe[1], body_for_cgi.c_str() + written, body_for_cgi.size() - written);
            if (n > 0) {
                written += static_cast<size_t>(n);
                if (written >= body_for_cgi.size()) {
                    close(_stdin_pipe[1]);
                    _stdin_pipe[1] = -1;
                    stdin_open = false;
                }
            } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                kill(_pid, SIGKILL);
                waitpid(_pid, NULL, 0);
                if (_stdin_pipe[1] != -1) { close(_stdin_pipe[1]); _stdin_pipe[1] = -1; }
                if (_stdout_pipe[0] != -1) { close(_stdout_pipe[0]); _stdout_pipe[0] = -1; }
                return false;
            }
        }

        if (stdout_open && FD_ISSET(_stdout_pipe[0], &readfds)) {
            char buf[4096];
            ssize_t n = read(_stdout_pipe[0], buf, sizeof(buf));
            if (n > 0) {
                _cgi_output.append(buf, static_cast<size_t>(n));
            } else if (n == 0) {
                close(_stdout_pipe[0]);
                _stdout_pipe[0] = -1;
                stdout_open = false;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                kill(_pid, SIGKILL);
                waitpid(_pid, NULL, 0);
                if (_stdin_pipe[1] != -1) { close(_stdin_pipe[1]); _stdin_pipe[1] = -1; }
                if (_stdout_pipe[0] != -1) { close(_stdout_pipe[0]); _stdout_pipe[0] = -1; }
                return false;
            }
        }
    }

    int status = 0;
    if (waitpid(_pid, &status, 0) < 0)
        return false;

    if (!parseCgiResponse())
        return false;

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        if (_response.empty())
            return false;
    }

    return true;
}

std::string CgiHandler::getResponse() const {
    return _response;
}
