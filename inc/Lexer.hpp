#ifndef LEXER_HPP
#define LEXER_HPP

#include <iostream>
#include <vector>
#include <stdexcept>

enum TokenType {
	T_WORD,
	T_LBRACE,
	T_RBRACE,
	T_SEMICOLON,
	T_EOF
};

struct Token {
	TokenType	type;
	std::string	value;
	size_t		line;
	Token(TokenType t, std::string v, size_t l) : type(t), value(v), line(l) {}
};

class Lexer {
	private:
		std::string	_src;
		size_t		_pos;
		size_t		_line;
		
		bool		isSpace(char c) const;
		bool		isSpecial(char c) const;
		void		skipWhitespace();
		Token		readQuotedString(char quote);
		Token		readWord();
	public:
		Lexer(const std::string& src);
		~Lexer();
		std::vector<Token> tokenize();
};

#endif
