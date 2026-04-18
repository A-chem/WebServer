#include "CGIConfig.hpp"

CGIConfig::CGIConfig() {}
CGIConfig::~CGIConfig() {}

void CGIConfig::addHandler(const std::string& extension, const std::string& executable) {
if (extension.empty() || executable.empty()) return;
std::string ext = extension;
if (ext[0] != '.') ext = "." + ext;
_handlers[ext] = executable;
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

if (uriPath.size() >= ext.size() &&
uriPath.compare(uriPath.size() - ext.size(), ext.size(), ext) == 0) {
size_t pos = uriPath.size() - ext.size();
size_t segStart = uriPath.rfind('/', pos);
if (segStart == std::string::npos) segStart = 0;
else segStart += 1;
if (pos > segStart) {
scriptName = uriPath;
pathInfo = "";
executable = it->second;
return true;
}
}

std::string marker = ext + "/";
size_t pos = uriPath.find(marker);
while (pos != std::string::npos) {
size_t segStart = uriPath.rfind('/', pos);
if (segStart == std::string::npos) segStart = 0;
else segStart += 1;
if (pos > segStart) {
size_t after = pos + ext.size();
scriptName = uriPath.substr(0, after);
pathInfo = uriPath.substr(after);
executable = it->second;
return true;
}
pos = uriPath.find(marker, pos + 1);
}
}
return false;
}

bool CGIConfig::empty() const {
return _handlers.empty();
}
