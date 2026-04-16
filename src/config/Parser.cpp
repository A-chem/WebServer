#include "Parser.hpp"

Parser::Parser(const std::vector<Token>& tokens) : _tokens(tokens), _pos(0) {}
Parser::~Parser() {}

ConfigNode*	Parser::parse() {
	ConfigNode* root = new ConfigNode(NODE_BLOCK);
	root->name = "ROOT";

	while (_pos < _tokens.size() && _tokens[_pos].type != T_EOF) {
		ConfigNode* child = parseStatement();
		child->parent = root;
		root->children.push_back(child);
	}
	return root;
}

ConfigNode*	Parser::parseStatement() {
	if (_pos >= _tokens.size()) return NULL;
	if (_tokens[_pos].type != T_WORD) {
		throw std::runtime_error("Syntax Error: Expected directive name");
	}
	std::string name = _tokens[_pos].value;
	_pos++;
	std::vector<std::string> args;
	while (_pos < _tokens.size() && _tokens[_pos].type == T_WORD) {
		args.push_back(_tokens[_pos].value);
		_pos++;
	}
// mn b3d manssaliw l args ida sala l file (tokens) so config file not valid
	if (_pos >= _tokens.size()) throw std::runtime_error("Unexpected End of file");
// ida l9ina ';' token so 3ndna leaf node otherwise ghadi tkon block directive 
	if (_tokens[_pos].type == T_SEMICOLON) {
		_pos++;
		return new ConfigNode(NODE_DIRECTIVE, name, args);
	}
	else if (_tokens[_pos].type == T_LBRACE) {
		_pos++;
		ConfigNode* node = new ConfigNode(NODE_BLOCK, name, args);
		//since we are inside a block directive here so ghadi ndkhlo f recursion 7ta nl9aw } fhad node aw branch aw li bghiti tssmiha .
		while (_pos < _tokens.size() && _tokens[_pos].type != T_RBRACE) {
			ConfigNode* child = parseStatement();
			child->parent = node;
			node->children.push_back(child);
		}
		if (_pos >= _tokens.size() || _tokens[_pos].type != T_RBRACE) {
			throw std::runtime_error("Syntax Error: Missing closing brace '}'");
		}
		_pos++;
		return node;
	}
	else 
	    throw std::runtime_error("Syntax Error: Expected ';' or '{'");
}

