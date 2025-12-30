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