#include "Server.hpp"
#include <sstream>
#include <sys/wait.h>


Server::Server() {
	epfd = epoll_create1(0);
	if (epfd == -1)
		throw std::runtime_error("epoll_create1 failed");
}

Server::~Server() {
	for (size_t i = 0; i < listenfd.size(); i++)
		close(listenfd[i]);
	for (std::map<int, Client*>::iterator it = clients.begin(); it != clients.end(); ++it) {
		close(it->first);
		delete it->second;
	}
	close(epfd);
}

// create listen sockets for every unique host:port for each server block in the config file 
// also build our tracker _fd_to_config bach n3rfo which ServerConfig mlinki m3a listen fd 
// so db kola connection ghadi nkhtaro liha server block dyalha (flowel knt mhard codi ga3 connections ikhdmo blconfig lowla )

void	Server::setup(const std::vector<ServerConfig>& configs) {
	_configs = configs;
	std::map<std::string, int> bound;

	for (size_t i = 0; i < _configs.size(); i++) {
		for (size_t j = 0; j < _configs[i].listen_sockets.size(); j++) {
			std::string host = _configs[i].listen_sockets[j].first;
			int	port = _configs[i].listen_sockets[j].second;

			std::ostringstream oss;
			oss << host << ":" << port;
			std::string key = oss.str();

			if (bound.find(key) == bound.end()) {
				int fd = createListenSocket(host, port);
				listenfd.push_back(fd);
				bound[key] = fd;
				addToEpoll(fd);
				std::cout << "[LISTEN]" << key << " fd=" << fd << std::endl;
			}
			int lfd = bound[key];
			if (_fd_to_config.find(lfd) == _fd_to_config.end())
				_fd_to_config[lfd] = static_cast<int>(i);
		}
	}
}

void	Server::acceptClients(int listen_fd) {
	while (true) {
		int clientfd = accept(listen_fd, NULL, NULL);
		if (clientfd < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) break;
			return ;
		}
		setNonBlocking(clientfd);
		addToEpoll(clientfd);
		Client* cl = new Client(clientfd);
		cl->setListenFd(listen_fd);
		clients.insert(std::make_pair(clientfd, cl));
		std::cout << "[ACCEPT] client fd=" << clientfd << std::endl;
	}
}

void	Server::disconnect(int fd) {
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);
	delete clients[fd];
	clients.erase(fd);
	std::cout << "[DISCONNECT] fd=" << fd << std::endl;
}

