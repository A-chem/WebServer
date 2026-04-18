#include "Server.hpp"

// adding fd to epoll for read event (EPOLLIN)

void	Server::addToEpoll(int fd) {
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = fd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

// if we have a response ready and we have to send 

void	Server::modifyEpoll(int fd, uint32_t events) {
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;
	epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev);
}

// check wach fd one of our listening sockets (checkih wach wdina hh)

bool	Server::isListenFd(int fd) {
	return std::find(listenfd.begin(), listenfd.end(), fd) != listenfd.end();
}


// main loop , epoll_wait until one fd is ready 

void	Server::eventLoop() {
	const int MAX_EVENTS = 1024;
	struct epoll_event events[MAX_EVENTS];

	while (true) {
		int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
		if (n <= 0) continue;

		for (int i = 0; i < n; ++i) {
			int fd = events[i].data.fd;
			// error or hangup clean up the client
			if (events[i].events & (EPOLLERR | EPOLLHUP)) {
				disconnect(fd);
				continue;
			}
			if (isListenFd(fd)) {
				// new connection
				acceptClients(fd);
			}
			else {
				Client& c = *clients[fd];
				if (c.getState() < PROCESS_REQUEST)
					handleRequest(fd);
				if (c.getState() == WRITE_RESPONSE)
					handleResponse(fd);
				if (c.getState() == CLOSED)
					disconnect(fd);
			}
		}
	}
}
