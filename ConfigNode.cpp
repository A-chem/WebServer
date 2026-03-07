#include "ConfigNode.hpp"

ConfigNode::ConfigNode(NodeType t, std::string n, std::vector<std::string> a, ConfigNode* p) 
    : type(t), name(n), args(a), parent(p) {}

ConfigNode::~ConfigNode() {
    for(size_t i = 0; i < children.size(); i++) {
        delete children[i];
    }
}
