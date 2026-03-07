#include "Server.hpp"
#include <sstream>

// --- CLIENT METHODS (Unchanged) ---
Client::Client(): fd(-1), state(READ_REQUEST_LINE), bytes_sent(0), header_sent(false) {}
Client::Client(int fd): fd(fd), state(READ_REQUEST_LINE), bytes_sent(0), header_sent(false) {}
Client::~Client() {}

int Client::getFd() const { return fd; }
std::string& Client::recvBuf() { return recv_buf; }
std::string& Client::sendBuf() { return send_buf; }
void Client::setState(State s) { state = s; }
State Client::getState() const { return state; }

bool Client::requestComplete() const {
    return recv_buf.find("\r\n\r\n") != std::string::npos;
}

// --- SERVER SETUP METHODS ---
Server::Server() {
    epfd = epoll_create1(0);
    if(epfd == -1)
        throw std::runtime_error("Error: epoll_create1 failed");
}

Server::~Server() {
    for(size_t i = 0; i < listenfd.size(); ++i)
        close(listenfd[i]);
    close(epfd);
}

void Server::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        throw std::runtime_error("fcntl F_GETFL failed");

    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("fcntl F_SETFL failed");
}

int Server::createListenSocket(const std::string& host, int port) {
    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo *it;

    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;
   
    // Convert int port to string for getaddrinfo (C++98 safe)
    std::ostringstream oss;
    oss << port;
    std::string port_str = oss.str();

    int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
    if (ret != 0)
        throw std::runtime_error("Error: getaddrinfo failed, " + std::string(gai_strerror(ret)));

    int fd = -1;
    for(it = res; it != NULL; it = it->ai_next) {
        fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
        if(fd < 0) continue;
        
        int yes = 1;
        if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
            close(fd);
            continue;
        }
        
        // Note: SO_REUSEPORT is not strictly standard on all older OSs, but harmless if defined.
        #ifdef SO_REUSEPORT
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes));
        #endif
        
        if(bind(fd, it->ai_addr, it->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    
    if (fd < 0) throw std::runtime_error("Error: bind failed for listen");
    if(listen(fd, SOMAXCONN) < 0) {
        close(fd);
        throw std::runtime_error("Error: listen failed");
    }
    setNonBlocking(fd);
   
    return fd;
}

void Server::setup(const std::vector<ServerConfig>& configs) {
    _configs = configs; // Save configs internally
    std::map<std::string, int> bound_sockets; // Track IP:Port to prevent duplicate binds

    for(size_t i = 0; i < _configs.size(); i++) {
        // Create a unique key like "127.0.0.1:8080"
        std::ostringstream oss;
        oss << _configs[i].host << ":" << _configs[i].port;
        std::string addr_key = oss.str();

        // Only bind if we haven't already bound to this IP:Port
        if (bound_sockets.find(addr_key) == bound_sockets.end()) {
            int fd = createListenSocket(_configs[i].host, _configs[i].port);
            listenfd.push_back(fd);
            bound_sockets[addr_key] = fd;
            std::cout << "[LISTEN] " << addr_key << " fd=" << fd << std::endl;
            addToEpoll(fd);
        }
    }
}

// --- EPOLL & CONNECTION MANAGEMENT ---
void Server::addToEpoll(int fd) {
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

void Server::modifyEpoll(int fd, uint32_t events) {
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

bool Server::isListenFd(int fd) {
    return std::find(listenfd.begin(), listenfd.end(), fd) != listenfd.end();
}

void Server::acceptClients(int listen_fd) {
    while(true) {
        int clientfd = accept(listen_fd, NULL, NULL);
        if(clientfd < 0) {
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            return;
        }
        setNonBlocking(clientfd);
        addToEpoll(clientfd);
        
        // We pass listen_fd here to potentially track which server block they hit
        clients.insert(std::make_pair(clientfd, new Client(listen_fd))); 
        std::cout << "Accepted Client fd = " << clientfd << std::endl;
    }
}

void Server::disconnect(int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    delete clients[fd];
    clients.erase(fd);
    std::cout << "Disconnected Client fd = " << fd << std::endl;
}

void Server::eventLoop() {
    const int MAX_EVENTS = 1024;
    struct epoll_event events[MAX_EVENTS];

    while(true) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
        if(n == 0) continue;
        for(int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;

            if(events[i].events & (EPOLLERR | EPOLLHUP)) {
                disconnect(fd);
                continue;
            }

            if(isListenFd(fd)) {
                acceptClients(fd);
            } else {
                Client &c = *clients[fd];
                if(c.getState() < PROCESS_REQUEST)
                    handleRequest(fd);
                if (c.getState() == WRITE_RESPONSE)
                    handleResponse(fd);
                if(c.getState() == CLOSED)
                    disconnect(fd);
            }
        }
    }
}
