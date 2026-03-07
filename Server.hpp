#ifndef SERVER_HPP
#define SERVER_HPP

#include "ConfigLoader.hpp" // Replaces ConfigNode!
#include <vector>
#include <string>
#include <map>
#include <netdb.h>
#include <fcntl.h>
#include <cerrno>
#include <unistd.h>
#include <sys/epoll.h>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstring>
#include <sys/stat.h>
#include <sstream>

#define READ_CHUNK 1024

enum State {
    READ_REQUEST_LINE,
    READ_REQUEST_HEADER,
    READ_BODY,
    PROCESS_REQUEST,
    WRITE_RESPONSE,
    CLOSED
};

class Client {
private:
    int         fd;
    std::string recv_buf;
    std::string send_buf;
    State       state;

    std::string method;
    std::string path;
    std::string version;

    std::map<std::string, std::string> header;
    std::string body;

    size_t  content_length;
    size_t  file_size;
    size_t  bytes_sent;
    bool    header_sent;
    
public:
    std::ifstream   file_stream;
    
    Client();
    Client(int fd);
    ~Client();

    int             getFd() const;
    std::string&    recvBuf();
    std::string&    sendBuf();
    void            setState(State s);
    State           getState() const;
    void            setMethod(std::string m);
    void            setPath(std::string p);
    void            setVersion(std::string v);
    void            setHeader(std::string key, std::string value);
    void            setFileSize(size_t  fs);
    void            setBytesSent(size_t bs);
    void            setHeaderSent(bool hs);
    
    std::map<std::string, std::string> getHeader() const;
    std::string     getMethod() const;
    std::string     getPath() const;
    size_t          getBytesSent() const;
    size_t          getFileSize() const;
    bool            requestComplete() const;
    bool            headerSent();
    
    void            openFile(const std::string &path);
    std::streamsize readFile(char *buf, std::size_t size);
};

class Server {
private:
    int                                     epfd;
    std::vector<int>                        listenfd;
    std::map<int, Client*>                  clients;
    std::vector<ServerConfig>               _configs; 

    int     createListenSocket(const std::string& host, int port); 
    void    setNonBlocking(int fd);
    void    addToEpoll(int fd);
    void    modifyEpoll(int fd, uint32_t events);
    bool    isListenFd(int fd);
    void    acceptClients(int fd);
    void    handleRequest(int fd);
    void    handleResponse(int fd);
    void    disconnect(int fd);
    void    buildResponse(Client& c);

public:
    Server();
    ~Server();
    void setup(const std::vector<ServerConfig>& configs); 
    void eventLoop();
};

#endif
