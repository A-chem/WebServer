#ifndef CGI_HPP
#define CGI_HPP

#include "Server.hpp"
#include <map>
#include <string>

struct CGIResult {
	int					status_code;
	std::map<std::string, std::string>	headers;
	std::string				body;

	CGIResult() : status_code(200) {}
};

class CGI {
	private:
		std::string	_script_path;
		std::string	_executable;
		const Client&	_client;
		const LocationConfig& _location;
		const ServerConfig&	_server;

		std::map<std::string, std::string>	buildEnvironment() const;
		static bool				parseOutput(const std::string& output, CGIResult& result);

	public:
		CGI(const std::string& scriptPath,
			const std::string& executable,
			const Client& client,
			const LocationConfig& location,
			const ServerConfig& server);
		~CGI();

		bool	execute(CGIResult& result, std::string& error) const;
};

#endif
