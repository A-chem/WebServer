#include "Server.hpp"
#include <sstream>

// --- CLIENT GETTERS & SETTERS ---
void Client::setMethod(std::string m) { method = m; }
void Client::setPath(std::string p) { path = p; }
void Client::setVersion(std::string v) { version = v; }
void Client::setHeader(std::string key, std::string value) { header[key] = value; }

std::string Client::getMethod() const { return method; }
std::string Client::getPath() const { return path; }
std::map<std::string, std::string> Client::getHeader() const { return header; }

// --- PARSING LOGIC ---
int parseRequestLine(Client& c) {
    size_t pos = c.recvBuf().find("\r\n");
    if (pos == std::string::npos) return -1;

    std::string line = c.recvBuf().substr(0, pos);

    size_t first_space = line.find(' ');
    if(first_space == std::string::npos) return -1;
    
    size_t second_space = line.find(' ', first_space + 1);
    if (second_space == std::string::npos) return -1;

    std::string method = line.substr(0, first_space);
    std::string path = line.substr(first_space + 1, second_space - first_space - 1);
    std::string version = line.substr(second_space + 1);

    c.setMethod(method);
    c.setPath(path);
    c.setVersion(version);
    return 0;
}

void parseHeaders(Client& c) {
    std::string &buf = c.recvBuf();
    size_t header_end = buf.find("\r\n\r\n");
    if (header_end == std::string::npos) return;

    std::string all_headers = buf.substr(0, header_end);
    std::stringstream ss(all_headers);
    std::string line;

    while(std::getline(ss, line) && line != "\r" && !line.empty()) {
        size_t colon = line.find(':');
        if(colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            
            // Trim leading spaces
            size_t start = value.find_first_not_of(" \t");
            if (start != std::string::npos) value = value.substr(start);
            
            // Trim trailing \r
            if(!value.empty() && value[value.size() - 1] == '\r')
                 value.erase(value.size() - 1, 1);
                 
            c.setHeader(key, value);
        }
    }
}

// --- REQUEST HANDLING ---
void Server::handleRequest(int fd) {
    Client &c = *clients[fd];
    char buf[4096];
    ssize_t n = recv(fd, buf, sizeof(buf), 0);

    if (n <= 0) {
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
            c.setState(CLOSED);
        return;
    }
    c.recvBuf().append(buf, n);

    while (true) {
        if(c.getState() == READ_REQUEST_LINE) {
            size_t pos = c.recvBuf().find("\r\n");
            if(pos != std::string::npos) {
                if(parseRequestLine(c) < 0) {
                    c.setState(CLOSED);
                    break;
                }
                c.recvBuf().erase(0, pos + 2);
                c.setState(READ_REQUEST_HEADER);
            } else {
                break;
            }
        }
        else if(c.getState() == READ_REQUEST_HEADER) {
            size_t pos = c.recvBuf().find("\r\n\r\n");
            if(pos != std::string::npos) {
                parseHeaders(c);
                c.recvBuf().erase(0, pos + 4);
                
                // For now, jump straight to PROCESS. 
                // Later, you can add `READ_BODY` for POST requests here!
                c.setState(PROCESS_REQUEST);
            } else {
                break;
            }
        }
        else if(c.getState() == PROCESS_REQUEST) {
            buildResponse(c);
            c.setState(WRITE_RESPONSE);
            modifyEpoll(fd, EPOLLOUT);
            break;
        }
        else {
            break;
        }
    }
}
