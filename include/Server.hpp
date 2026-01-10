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
     //   size_t cmbs;
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
        std::string root;
        std::vector<std::string> index;
        size_t cmbs;
        std::map<int, std::string> error_pages;
        std::vector<LocationConf> locations;

        public:
        ServerConf();
        ~ServerConf();
        void parseServer(const std::vector<std::string> &tokens, size_t& i, ServerConf& server);
        void setCmds(size_t val);
        size_t getCmbs() const { return cmbs; };
        void display() const {
        std::cout << "Server Configuration:\n";

            std::cout << "Listen addresses:\n";
            for (size_t j = 0; j < listen.size(); ++j)
                std::cout << "  " << listen[j].first << ":" << listen[j].second << "\n";
            std::cout << "Root: " << root << "\n";

            std::cout << "Index files:\n";
            for (size_t j = 0; j < index.size(); ++j)
                std::cout << "  " << index[j] << "\n";

            std::cout << "Client max body size: " << cmbs << "\n";

            std::cout << "Error pages:\n";
            for (std::map<int, std::string>::const_iterator it = error_pages.begin(); it != error_pages.end(); ++it)
                std::cout << "  " << it->first << " -> " << it->second << "\n";

            // std::cout << "Locations:\n";
            // for (size_t j = 0; j < locations.size(); ++j)
            //     locations[j].print();
    }
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