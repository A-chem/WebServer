#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <map>
#include <vector>
#include <ctime>
#include <sys/types.h>

class Cgi {
	private:
		pid_t					_pid;
		int						_inFd;
		int						_outFd;
		size_t					_bodySent;
		time_t					_startTime;
		bool					_bodyDone;
		bool					_outputDone;
		std::string				_body;
		std::string				_output;
		std::string				_requestMethod;
		std::string				_contentType;
		size_t					_contentLength;
		std::string				_scriptPath;
		std::string				_scriptUri;
		std::string				_queryString;
		std::string				_cgiBin;
		std::string				_docRoot;
		std::string				_serverName;
		std::string				_serverPort;
		std::string				_remoteAddr;
		std::string				_remotePort;
		std::map<std::string, std::string>	_headers;

		bool	buildEnv(std::vector<std::string>& envStore, const std::string& absScriptPath) const;

	public:
		Cgi();
		~Cgi();

		void	setRequestMethod(const std::string& method);
		void	setContentType(const std::string& contentType);
		void	setContentLength(size_t contentLength);
		void	setBody(const std::string& body);
		void	addHeader(const std::string& key, const std::string& value);
		void	setServerInfo(const std::string& name, const std::string& port);
		void	setClientInfo(const std::string& addr, const std::string& port);

		bool	initialize(const std::string& scriptPath, const std::string& scriptUri,
						const std::string& queryString, const std::string& cgiBin,
						const std::string& docRoot);
		bool	sendBody();
		bool	readOutput();
		bool	hasTimedOut() const;
		void	terminate();
		void	cleanup();

		const std::string&	getOutput() const;

		static bool	parseResponse(const std::string& raw, int& code, std::string& reason,
								std::string& contentType, std::string& body,
								std::vector<std::string>& extraHeaders);
};

#endif
