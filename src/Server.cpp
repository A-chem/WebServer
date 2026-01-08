#include "../include/Server.hpp"
#include "../include/Utils.hpp"

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
        if(tokens[i] == "server")
        {
            i++;
            if(tokens[i++] != "{") 
                throw std::runtime_error("Expected { after server");
        
        ServerConf server;
        server.parseServer(tokens, i, server);
        //config.servers.puch.back(server);
        }
        // else
        //     throw std::runtime_error("Unexpected token outside server: " + tokens[i]);
        break ;
    }
}

Conf::~Conf(){};

ServerConf:: ServerConf() : index("index.html"), cmbs(0){};

ServerConf::~ServerConf(){};



void ServerConf::parseServer(const std::vector<std::string> &tokens, size_t& i, ServerConf& server)
{
    int s = 0;
    while(i < tokens.size() && tokens[i] != "}")
    {
        std::string token = tokens[i];
        if(token == "listen")
        {
            i++;
            std::string value = tokens[i++];
            if(tokens[i] != ";") throw std::runtime_error("Missing ; after listen");
            server.listen.push_back(parseListenValue(value));
        }
        else if(token == "root") server.root = parseSingleValue(tokens, i);
        else if (token == "index") server.index = parseSingleValue(tokens, i);
        else if (token == "client_max_body_size") server.cmbs = parseSize(tokens, i);
        else if (token == "error_page") 
        {
            i++;
            int code = std::atoi(tokens[i].c_str());
            i++;
            std::string path = tokens[i++];
            if (tokens[i++] != ";") throw std::runtime_error("Missing ;");
            server.error_pages[code] = path;
            s++;
        }
        i++;
    }
}
