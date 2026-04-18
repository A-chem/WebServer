#include "ConfigLoader.hpp"
#include "Utils.hpp"
#include <cstdlib>

ConfigLoader::ConfigLoader() {}
ConfigLoader::~ConfigLoader() {}

// hna function bach kan joini lpaths dyal child ol parent btari9a ss7i7a kan 3ndi had lmochkil ( parent = /www/ + child = /folderx = /www//folderx )
std::string	joinPaths(const std::string& parent, const std::string& child) {
	if (parent.empty() || parent == "/") return child;
	if (child.empty()) return parent;

	if(parent[parent.size() - 1] == '/' && child[0] == '/') {
		return parent + child.substr(1);
	}
	if (parent[parent.size() - 1] != '/' && child[0] != '/') {
		return parent + "/" + child;
	}
	return parent + child;
}

std::vector<ServerConfig>	ConfigLoader::loadServers(ConfigNode* root) {
	std::vector<ServerConfig> servers;
	if (!root || root->type != NODE_BLOCK) return servers;

	for (size_t i = 0; i < root->children.size(); i++) {
		if (root->children[i]->name == "server") {
			ServerConfig conf;
			loadServer(root->children[i], conf);
			servers.push_back(conf);
		}
	}
	return servers;
}

void	ConfigLoader::loadServer(ConfigNode* node, ServerConfig& conf) {
	std::string	serverRoot;
	std::string	serverIndex;

	for (size_t i = 0; i < node->children.size(); i++) {
		ConfigNode* child = node->children[i];

		if (child->name == "listen") {
			conf.listen_sockets.push_back(parseListen(child->args[0]));
		}
		else if (child->name == "server_name") {
			conf.server_names = child->args;
		}
		else if (child->name == "client_max_body_size") {
			conf.client_max_body_size = parseSize(child->args[0]);
		}
		else if (child->name == "error_page") {
			int code = std::atoi(child->args[0].c_str());
			conf.error_pages[code] = child->args[1];
		}
		else if (child->name == "cgi") {
			conf.cgi.parseDirective(child->args);
		}
		else if (child->name == "root") serverRoot = child->args[0];
		else if (child->name == "index") serverIndex = child->args[0];
	}
	if (conf.listen_sockets.empty()) {
		conf.listen_sockets.push_back(std::make_pair("0.0.0.0", 80));
	}

	LocationConfig	defaultLoc;
	defaultLoc.path = "/";
	defaultLoc.root = serverRoot;
	defaultLoc.index = serverIndex;
	defaultLoc.client_max_body_size = conf.client_max_body_size;
	defaultLoc.error_pages = conf.error_pages;
	defaultLoc.cgi = conf.cgi;

	// start load the location config here and pass what can inherit from the parent node (in this case root) but conf is gonna be used inside loadlocation too 
	for (size_t i = 0; i < node->children.size(); i++) {
		if (node->children[i]->name == "location") {
			loadLocation(node->children[i], defaultLoc, conf.locations);
		}
	}
}


void	ConfigLoader::loadLocation(ConfigNode* node, LocationConfig parent, std::vector<LocationConfig>& list) {
	LocationConfig	loc = parent;
	loc.path = joinPaths(parent.path, node->args[0]);

	for (size_t i = 0; i < node->children.size(); i++) {
	ConfigNode* child = node->children[i];

	if (child->name == "root") loc.root = child->args[0];
	else if (child->name == "index") loc.index = child->args[0];
	else if (child->name == "autoindex") loc.autoindex = (child->args[0] == "on");
	else if (child->name == "client_max_body_size") loc.client_max_body_size = parseSize(child->args[0]);
	else if (child->name == "return") loc.return_url = std::make_pair(std::atoi(child->args[0].c_str()), child->args[1]);
	else if (child->name == "cgi_pass") loc.cgi_pass = child->args[0];
	else if (child->name == "cgi") loc.cgi.parseDirective(child->args);
	else if (child->name == "upload_store") loc.upload_store = child->args[0];
	else if (child->name == "limit_except") { loc.allowed_methods = child->args; }
	else if (child->name == "location") { loadLocation(child, loc, list);}
	}
	list.push_back(loc);
}
