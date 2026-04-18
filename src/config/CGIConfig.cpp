#include "CGIConfig.hpp"

CGIConfig::CGIConfig() {}
CGIConfig::~CGIConfig() {}

void CGIConfig::addHandler(const std::string& extension, const std::string& executable) {
if (extension.empty() || executable.empty()) return;
_handlers[extension] = executable;
}

bool CGIConfig::parseDirective(const std::vector<std::string>& args) {
if (args.size() != 2) return false;
addHandler(args[0], args[1]);
return true;
}

bool CGIConfig::resolveScript(const std::string& uriPath,
std::string& scriptName,
std::string& pathInfo,
std::string& executable) const {
for (std::map<std::string, std::string>::const_iterator it = _handlers.begin(); it != _handlers.end(); ++it) {
const std::string& ext = it->first;
if (ext.empty()) continue;

size_t pos = uriPath.find(ext);
while (pos != std::string::npos) {
size_t after = pos + ext.size();
if (after == uriPath.size() || uriPath[after] == '/') {
scriptName = uriPath.substr(0, after);
pathInfo = (after < uriPath.size()) ? uriPath.substr(after) : "";
executable = it->second;
return true;
}
pos = uriPath.find(ext, pos + 1);
}
}
return false;
}

bool CGIConfig::empty() const {
return _handlers.empty();
}
