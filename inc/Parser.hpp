#ifndef PARSER_HPP
#define PARSER_HPP

#include "Lexer.hpp"
#include "ConfigNode.hpp"

class	Parser {
	private:
		std::vector<Token>	_tokens;
		size_t			_pos;

		ConfigNode*	parseStatement();
	public:
		Parser(const std::vector<Token>& tokens);
		~Parser();

		ConfigNode* parse();
};

#endif
