#ifndef CGICONFIG_HPP
#define CGICONFIG_HPP

#include <map>
#include <string>
#include <vector>

class CGIConfig {
private:
std::map<std::string, std::string> _handlers;

public:
CGIConfig();
~CGIConfig();

void addHandler(const std::string& extension, const std::string& executable);
bool parseDirective(const std::vector<std::string>& args);
bool resolveScript(const std::string& uriPath,
std::string& scriptName,
std::string& pathInfo,
std::string& executable) const;
bool empty() const;
};

#endif
