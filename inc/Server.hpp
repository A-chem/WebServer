#ifndef SERVER_HPP
#define SERVER_HPP

#include "ConfigLoader.hpp"
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <netdb.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <dirent.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>


// state machine (all the states that we gonna go through in each connection)

enum State {
	READ_REQUEST_LINE,
	READ_REQUEST_HEADER,
	READ_BODY,
	PROCESS_REQUEST,
	CGI_INIT,   
	CGI_EXEC,  
	CGI_WAIT,
	WRITE_RESPONSE,
	CLOSED
};

class Cgi;



class	Client {
	private:
		int		fd;
		int		listen_fd;
		std::string	recv_buf;
		std::string	send_buf;
		State		state;

		// parsed request (the data came from the client :browser)
		std::string	method;
		std::string	path;
		std::string	version;
		std::map<std::string, std::string> header;
		std::string	body;
		size_t		content_length;
		int		error_code;

		// Response streaming
		size_t	file_size;
		size_t	bytes_sent;
		bool	header_sent;
		bool	keep_alive;

		Cgi*		cgi;

	public:
		std::ifstream	file_stream;
		
		Client();
		Client(int fd);
		~Client();

		// for re-use the same connection (keep-alive)
		void	reset();

		//connection identity
		int	getFd() const;
		int	getListenFd() const;
		void	setListenFd(int lfd);

		// state machine
		std::string&		recvBuf();
		std::string&		sendBuf();
		void			setState(State s);
		State			getState() const;
		bool			requestComplete() const;

		// request setters
		void	setMethod(std::string m);
		void	setPath(std::string p);
		void	setVersion(std::string v);
		void	setHeader(std::string key, std::string value);
		void	setBody(const std::string& b);
		void	setContentLength(size_t cl);
		void	setErrorCode(int code);
		void	setKeepAlive(bool ka);

		// request getters
		std::string				getMethod()	const;
		std::string				getPath()	const;
		std::string				getVersion()	const;
		std::map<std::string, std::string>	getHeader()	const;
		std::map<std::string, std::string>&	getHeaderRef();
		void					removeHeader(const std::string& key);
		std::string				getBody()	const;
		size_t					getContentLength() const;
		int					getErrorCode()	const;
		bool					isKeepAlive()	const;

		// response file streaming
		void		openFile(const std::string& path);
		std::streamsize readFile(char * buf, std::size_t size);
		void		setFileSize(size_t fs);
		void		setBytesSent(size_t n);
		void		setHeaderSent(bool hs);
		bool		headerSent();
		size_t		getBytesSent() const;
		size_t		getFileSize() const;

		Cgi*		getCgi();
		void		setCgi(Cgi* c);
		void		clearCgi();
};


class Server {
	private:
		int				epfd;
		std::vector<int>		listenfd;
		std::map<int, Client*>		clients;
		std::vector<ServerConfig>	_configs;
		std::map<int, int>		_fd_to_config;

		// Socket.cpp methods
		void	setNonBlocking(int fd);
		int	createListenSocket(const std::string& host, int port);

		// Epoll.cpp
		void	addToEpoll(int fd);
		void	modifyEpoll(int fd, uint32_t events);
		bool	isListenFd(int fd);

		// Server.cpp
		void	acceptClients(int listen_fd);
		void	disconnect(int fd);

		// Request.cpp
		void	handleRequest(int fd);

		// Response.cpp
		void	buildResponse(Client& c);
		void	handleResponse(int fd);
		bool	resolveCgiInterpreter(const LocationConfig& loc, const std::string& path, std::string& cgiBin) const;

		// ErrorPages.cpp
		std::string defaultErrorPage(int code, const std::string& msg);
		std::string buildErrorResponse(int code, const std::string& msg, ServerConfig& srv);

	public:
		Server();
		~Server();
		void	setup(const std::vector<ServerConfig>& configs);
		void	eventLoop();
};

#endif
