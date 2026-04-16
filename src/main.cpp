#include "Lexer.hpp"
#include "Parser.hpp"
#include "ConfigValidator.hpp"
#include "ConfigLoader.hpp"
#include "Server.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int ac, char **av) {
	std::string config_file;

	if (ac == 1) {
		config_file = "configs/default_con.conf";
		std::cout << "No config file specified. using default: " << config_file << std::endl;
	} else if (ac == 2) {
		config_file = av[1];
	} else {
		std::cerr << "Usage: " << av[0] << " [path/to/configfile.conf]" << std::endl;
		return 1;
	}

	std::ifstream file(config_file.c_str());
	if (!file.is_open()) {
		std::cerr << "Error: couldn't open file : " << config_file << std::endl;
		return 1;
	}

	std::ostringstream ss;
	ss << file.rdbuf();
	std::string config_text = ss.str();
	file.close();

	if (config_text.empty()) {
		std::cerr << "Error: config file is empty" << std::endl;
		return 1;
	}

	try {
		std::cout << "------ 1. Lexing ----------" << std::endl;
		Lexer lexer(config_text);
		std::vector<Token> tokens = lexer.tokenize();

		std::cout << "------ 2. Parsing ---------" << std::endl;
		Parser parser(tokens);
		ConfigNode* ast_root = parser.parse();

		std::cout << "------ 3. Validating ------" << std::endl;
		ConfigValidator validator;
		validator.validate(ast_root);

		std::cout << "------ 4. Loading conf ----" << std::endl;
		ConfigLoader loader;
		std::vector<ServerConfig> configs = loader.loadServers(ast_root);

		delete ast_root;
		// this is added just to check if the data loaded correctly

		std::cout << "Loaded" << configs.size() << " server(s) " << std::endl;
		for (size_t i = 0; i < configs.size(); i++) {
			std::cout << "Server " << i << "has" 
				<< configs[i].locations.size() << "location(s)" << std::endl;
			for (size_t j = 0; j < configs[i].locations.size(); j++) {
				std::cout << "location: " << configs[i].locations[j].path
					<< " root: " << configs[i].locations[j].root << std::endl;
			}
		}
		/////////////////////////////////////////////////////////////////////
	if (configs.empty())
    		throw std::runtime_error("No valid server blocks found in config file.");

	std::cout << "--- 5. Starting Server ---" << std::endl;
	Server my_server;
	my_server.setup(configs);
	std::cout << "Server is running! Waiting for connections..." << std::endl;
	my_server.eventLoop();
	} catch (const std::exception& e) {
		std::cerr << "\n Fatal error during setup: " <<  e.what() << std::endl;
		return 1;
	}
}
