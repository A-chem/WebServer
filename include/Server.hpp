#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <exception>
#include <vector>
#include <algorithm>
#include <utility> 
#include <map>

class CgiConf
{
    public:
        std::string extension;
        std::string binary;
};
class LocationConf
{
    private:
        std::string path;
        std::vector<std::string> methods;
        std::string root;
        std::string index;
        bool autoindex;
        std::string   upload_path;
        std::pair<int, std::string> redirect;
        //std::vector<CgiConf> cgis;

        public:
       LocationConf()
        : autoindex(false), redirect(std::make_pair(0, "")){};
        ~LocationConf(){};
        void setAutoindex(bool value) { autoindex = value; }
        bool getAutoindex() const { return autoindex; }
};
class ServerConf
{
    private:
        std::vector<std::pair<std::string, int> > listen;
        std::string port;
        std::string root;
        std::string index;
        size_t cmbs;
        std::map<int, std::string> error_pages;
        std::vector<LocationConf> locations;

        public:
        ServerConf();
        ~ServerConf();
        void parseServer(const std::vector<std::string> &tokens, size_t& i, ServerConf& server);
         size_t getCmbs() const { return cmbs; };
};

class Conf
{
    private:
        std::vector<ServerConf> servers;
    public:
        Conf();
        ~Conf();
    const std::vector<ServerConf>& getServers() const;
    void parseConfig(const std::vector<std::string> &tokens, Conf& config);
};

#endif