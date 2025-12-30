#include "../include/Utils.hpp"

std::vector<std::string> tokenize(const std::string &content)
{
    std::vector<std::string> tokens;
    std::string token;

    for (size_t i = 0; i < content.size(); i++)
    {
        char c = content[i];

        if (isspace(c))
        {
            if (!token.empty())
            {
                tokens.push_back(token);
                token = "";
            }
        }
        else if (c == '{' || c == '}' || c == ';')
        {
            if (!token.empty()) { tokens.push_back(token); token = ""; }
            std::string s(1, c);
            tokens.push_back(s);
        }
    
        else
        {
            token += c; 
        }
    }

    if (!token.empty()) tokens.push_back(token);

    return (tokens);
}

bool isValidIPv4(const std::string &ip)
{
      std::istringstream ss(ip);
    std::string token;
    int count = 0;

    while (std::getline(ss, token, '.')) {
        if (token.empty()) return false;
        for (size_t i = 0; i < token.size(); ++i)
            if (token[i] < '0' || token[i] > '9') return false;

        int num = std::atoi(token.c_str());
        if (num < 0 || num > 255) return false;
        count++;
    }
    return count == 4;
}

bool isValidHostname(const std::string &host)
{
    if(host.empty() || host.size() > 253) return (false);
    size_t len = 0;
    for(size_t i = 0; i < host.size(); ++i)
    {
        char  c = host[i];
         if (c == '.') 
         {
            if (len == 0) return (false);
            len = 0;
         }
         else if(std::isalnum(c) || c == '-') len++;
         else return (false);
    }
    return (len > 0);
}


std::pair<std::string, int> parseListenValue(const std::string &value)
{
    std::string host = value;
    int port = 0;

    size_t pos = value.find(':');
    if (pos != std::string::npos) {
        host = value.substr(0, pos);
        std::string port_str = value.substr(pos + 1);
        port = std::atoi(port_str.c_str());
    } else throw std::runtime_error("Listen value must contain ':'");

    if (!isValidIPv4(host) && !isValidHostname(host)) throw std::runtime_error("Invalid host");
    if (port <= 0 || port > 65535) throw std::runtime_error("Invalid listen port");
    return std::make_pair(host, port);
}

std::string parseSingleValue(const std::vector<std::string> &tokens, size_t &i)
{
    i++;
    if(i >= tokens.size()) throw std::runtime_error("Expected value");
    std::string val = tokens[i];
    i++;
    if(i >= tokens.size() || tokens[i] != ";") throw std::runtime_error("Missing ;");
    return  (val);
}
size_t parseSize(const std::vector<std::string> &tokens, size_t &i)
{
    std::string s = parseSingleValue(tokens, i);
    if (s.empty())
        throw std::runtime_error("Empty size value");
    size_t nbr = 1;
    char c = s[s.length()-1];

    if (c == 'K' || c == 'M' || c == 'G')
    {
        if (s.length() == 1)
            throw std::runtime_error("Missing number before unit");
        if (c == 'K') nbr = 1024;
        else if (c == 'M') nbr = 1024 * 1024;
        else if (c == 'G') nbr = 1024 * 1024 * 1024;
        s = s.substr(0, s.length() - 1);
    }
         for (size_t j = 0; j < s.length(); ++j)
            if (!std::isdigit(s[j])) throw std::runtime_error("Invalid size format: " + s);
        size_t val = std::strtoul(s.c_str(), NULL, 10);
        if (val == 0) throw std::runtime_error("Size must be greater than 0");
    return (val * nbr);
}