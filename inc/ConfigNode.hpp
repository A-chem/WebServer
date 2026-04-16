#ifndef CONFIG_NODE_HPP
#define CONFIG_NODE_HPP

#include <string>
#include <vector>

enum NodeType {
	NODE_BLOCK,
	NODE_DIRECTIVE
};

struct ConfigNode {
	NodeType			type;
	std::string			name;
	std::vector<std::string>	args;
	ConfigNode*			parent;
	std::vector<ConfigNode*>	children;
	ConfigNode(NodeType t, std::string n = "", std::vector<std::string> a = std::vector<std::string>(), ConfigNode* p = NULL);
	~ConfigNode();
};

#endif
