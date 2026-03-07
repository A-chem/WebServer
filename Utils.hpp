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

bool isNumeric(const std::string &s);

// Validation helpers
void parseListenValue(const std::string& vl);
void parse_http_code(const std::string& nb);
void parseCbmz(const std::string& vl);

// Extraction helpers (used by ConfigLoader)
std::pair<std::string, int> parseListen(const std::string& val);
size_t parseSize(const std::string& val);

#endif
