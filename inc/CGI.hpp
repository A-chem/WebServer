#ifndef CGI_HPP
#define CGI_HPP

#include "ConfigLoader.hpp"
#include <map>
#include <string>
#include <vector>
#include <sys/types.h>

class Client;

enum CGIState {
CGI_INIT,
CGI_FORK,
CGI_EXEC,
CGI_WRITE,
CGI_READ,
CGI_CLEANUP,
CGI_DONE,
CGI_ERROR
};

class CGI {
private:
CGIState _state;
pid_t _pid;
int _inPipe[2];
int _outPipe[2];
size_t _writeOffset;
std::string _body;
std::string _output;
std::map<std::string, std::string> _headers;
std::string _responseBody;
int _statusCode;
std::string _statusText;

void closeFd(int& fd);
void cleanup(bool reap);
bool setNonBlocking(int fd);
bool writeBodyToChild();
bool readChildOutput();
void parseOutput();
std::string buildResponse(const Client& c) const;

public:
CGI();
~CGI();

bool execute(const Client& c,
const std::string& scriptName,
const std::string& scriptFilename,
const std::string& pathInfo,
const std::string& executable,
std::string& httpResponse,
std::string& errorMessage);
};

#endif
