#include "../include/Server.hpp"
#include "../include/Utils.hpp"


int main()
{
    try
    {
    std::ifstream file("./conf/default.conf");
    std::stringstream ss;
    ss <<  file.rdbuf();
    std::string content = ss.str();

    std::vector<std::string> tokens = tokenize(content);

    Conf config;
    config.parseConfig(tokens, config);
    }
    catch(const std::exception& e)
    {
        std::cout << e.what() << std::endl;
    }
    //std::cout << "Parsed " <<  config.getServers().size()  << " servers.\n";

}