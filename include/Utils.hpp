#ifndef UTILS_HPP
#define UTILS_HPP


#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <exception>
#include <vector>
#include <algorithm>
#include <utility> 
#include <map>
#include <cstdlib>

std::vector<std::string> tokenize(const std::string &content);
std::pair<std::string, int> parseListenValue(const std::string &value);
bool isValidIPv4(const std::string &ip);
bool isValidHostname(const std::string &host);
std::string parseSingleValue(const std::vector<std::string> &tokens, size_t &i);
size_t parseSize(const std::vector<std::string> &tokens, size_t &i) ;

#endif