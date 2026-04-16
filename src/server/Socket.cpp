#include "Server.hpp"
#include <sstream>


// make a fd non-blocking 
void	Server::setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("fcntl F_GETFL Failed");
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("fcntl F_SETFL failed");
}

// creates a tcp socket , bind it to host:port and start listening

int	Server::createListenSocket(const std::string& host, int port) {
	struct	addrinfo hints;
	struct	addrinfo* res;

	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	std::ostringstream oss;
	oss << port;
	std::string port_str = oss.str();

	int ret = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
	if (ret != 0)
		throw std::runtime_error("getaddrinfo failed: " + std::string(gai_strerror(ret)));
	int fd = -1;

	for (struct addrinfo* it = res; it != NULL; it = it->ai_next) {
		fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (fd  < 0) continue;

		int yes = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

		if (bind(fd, it->ai_addr, it->ai_addrlen) == 0)
			break ;
		close(fd);
		fd = -1;
	}
	freeaddrinfo(res);

	if (fd < 0)
		throw std::runtime_error("bind failed for " + host + ":" + port_str);
	if (listen(fd, SOMAXCONN) < 0) {
		close(fd);
		throw std::runtime_error("listen failed");
	}

	setNonBlocking(fd);
	return fd;
}
