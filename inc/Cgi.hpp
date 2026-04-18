#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <vector>
#include <sys/types.h>

class Client;
struct ServerConfig;

class CgiHandler {
private:
    int _stdin_pipe[2];
    int _stdout_pipe[2];
    pid_t _pid;
    std::string _cgi_output;
    std::string _response;

    bool buildEnv(Client& client,
                  const ServerConfig& srv,
                  const std::string& uri_path,
                  const std::string& script_path,
                  std::vector<std::string>& env_storage,
                  std::vector<char*>& envp,
                  const std::string& body_for_cgi);
    std::string decodeChunked(const std::string& chunked, bool& ok) const;
    bool parseCgiResponse();
    bool setNonBlocking(int fd);

public:
    CgiHandler();
    ~CgiHandler();

    bool execute(Client& client,
                 const ServerConfig& srv,
                 const std::string& uri_path,
                 const std::string& script_path,
                 const std::string& interpreter);

    std::string getResponse() const;
};

#endif
