#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
#include <utility>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

bool	isNumeric(const std::string &s);

// validation helpers
void	parseListenValue(const std::string& vl); /* drna hadi wakha 3ndna parseListen bach ntjnbo copling */
void	parse_http_code(const std::string& nb);
void	parseCbmz(const std::string& vl); /* parse client max body size haha nice naming but ha hiya hna */

// extraction helpers
std::pair<std::string, int> parseListen(const std::string& val);
size_t	parseSize(const std::string& val);

#endif
