#include "../include/Server.hpp"

Conf::Conf(){};

const std::vector<ServerConf>& Conf::getServers() const
{
    return (servers);
}

void Conf::parseConfig(const std::vector<std::string> &tokens, Conf& config)
{
    (void) config;
    size_t i = 0;
    while(i < tokens.size())
    {
        std::cout << tokens[i] <<std::endl;
        if(tokens[i] == "server")
        {
            i++;
            if(tokens[i++] != "{") 
                throw std::runtime_error("Expected { after server");
        
            std::cout << "correct" <<std::endl;
            break;
        // ServerConf server;
        // parseServer(token, i, server);
        //config.servers.puch.back(server);
        }
        else
            throw std::runtime_error("Unexpected token outside server: " + tokens[i]);
    }
}

Conf::~Conf(){};

ServerConf:: ServerConf() : index("index.html"), cmbs(0){};

ServerConf::~ ~ServerConf(){};

void ServerConf::parseServer(const std::vector<std::string> &tokens, size_t& i, ServerConf& server)
{
    while(i < tokens.size() && tokens[i] != "}")
    {
        
    }
}
