#ifndef CONFIG_LOADER_HPP
#define CONFIG_LOADER_HPP

#include "ConfigNode.hpp"
#include <string>
#include <vector>
#include <map>
#include <utility>

struct	LocationConfig {
	std::string				path;
	std::string				root;
	std::string				index;
	bool					autoindex;
	size_t					client_max_body_size;
	std::pair<int, std::string>		return_url;
	std::string				cgi_pass;
	std::string				upload_store;
	std::vector<std::string>		allowed_methods;
	std::map<int, std::string>		error_pages;

	LocationConfig() : autoindex(false), client_max_body_size(0) {}
};

struct	ServerConfig {
	std::string					host;
	int						port;
	std::vector<std::pair<std::string, int> >	listen_sockets;
	std::vector<std::string>			server_names;
	size_t						client_max_body_size;
	std::map<int, std::string>			error_pages;
	std::vector<LocationConfig>			locations;

	ServerConfig() : host("0.0.0.0"), port(80), client_max_body_size(0) {}
};

class	ConfigLoader {
	private:
		void	loadServer(ConfigNode* node, ServerConfig& conf);
		void	loadLocation(ConfigNode* node, LocationConfig parent, std::vector<LocationConfig>& list);
	public:
		ConfigLoader();
		~ConfigLoader();

		std::vector<ServerConfig> loadServers(ConfigNode* root);
};

std::string	joinPaths(const std::string& parent, const std::string& child);

#endif
