#include "Server.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "ConfigValidator.hpp"
#include "ConfigLoader.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char** argv) {
    std::string config_file;

    // 1. Check command-line arguments
    if (argc == 1) {
        config_file = "configs/test.conf";
        std::cout << "No config file specified. Using default: " << config_file << std::endl;
    } else if (argc == 2) {
        config_file = argv[1];
    } else {
        std::cerr << "Usage: " << argv[0] << " [path/to/config.conf]" << std::endl;
        return 1;
    }

    // 2. Open and read the configuration file
    std::ifstream file(config_file.c_str()); // .c_str() is required for ifstream in C++98
    if (!file.is_open()) {
        std::cerr << "Error: Could not open config file: " << config_file << std::endl;
        return 1;
    }

    // Read the entire file into a string
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string config_text = ss.str();
    file.close();

    if (config_text.empty()) {
        std::cerr << "Error: Config file is empty." << std::endl;
        return 1;
    }

    // 3. Run the parsing and server pipeline
    try {
        std::cout << "--- 1. Lexing ---" << std::endl;
        Lexer lexer(config_text);
        std::vector<Token> tokens = lexer.tokenize();

        std::cout << "--- 2. Parsing ---" << std::endl;
        Parser parser(tokens);
        ConfigNode* ast_root = parser.parse();

        std::cout << "--- 3. Validating ---" << std::endl;
        ConfigValidator validator;
        validator.validate(ast_root);

        std::cout << "--- 4. Loading Config ---" << std::endl;
        ConfigLoader loader;
        std::vector<ServerConfig> configs = loader.loadServers(ast_root);
        
        // Clean up the raw AST now that we have our parsed structs
        delete ast_root;

        if (configs.empty()) {
            throw std::runtime_error("No valid server blocks found in config file.");
        }

        std::cout << "--- 5. Starting Server ---" << std::endl;
        Server my_server;
        my_server.setup(configs);
        
        std::cout << "Server is running! Waiting for connections..." << std::endl;
        my_server.eventLoop();

    } catch (const std::exception& e) {
        std::cerr << "\nFatal Error during setup: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
