#ifndef CGI_HPP
#define CGI_HPP

#include <string>
#include <map>
#include <unistd.h>
#include <sys/types.h>
#include <ctime>
#include <cerrno> 
#include <cstring> 

enum CGIState {
	CGI_IDLE,
	CGI_FORK,
	CGI_READING,
	CGI_PARSE_HEADER,
	CGI_PARSE_BODY,
	CGI_DECHUNK,
	CGI_FINISHED,
	CGI_TIMEOUT,
	CGI_ERROR
};

class CGI {
	private:
		pid_t		_pid;
		int			_pipe_out[2];
		int			_pipe_in[2];
		std::string	_raw_output;
		std::string	_headers;
		std::string	_body;
		CGIState	_state;
		time_t		_start_time;
		int			_timeout;
		int			_http_status;
		bool		_is_chunked;
		
		std::string	_method;
		std::string	_path;
		std::string	_query;
		std::string	_body_in;
		std::string	_content_type;
		std::string	_host;
		
		void		_setState(CGIState state);
		void		_cleanup();
		size_t		_hexToSize(const std::string& hex);
		std::string	_dechunkBody(const std::string& chunked);
		void		_parseHeadersState();
		void		_parseBodyState();
		void		_dechunkState();
		
	public:
		CGI();
		~CGI();
		
		void		setMethod(const std::string& m);
		void		setPath(const std::string& p);
		void		setQuery(const std::string& q);
		void		setBody(const std::string& b);
		void		setContentType(const std::string& ct);
		void		setHost(const std::string& h);
		
		int			execute(const std::string& script, int timeout = 30);
		
		const char*	getStateName() const;
		CGIState	getState() const;
		bool		isRunning() const;
		bool		isFinished() const;
		bool		isTimedOut() const;
		bool		hasError() const;
		
		std::string	getBody() const;
		std::string	getHeaders() const;
		int			getStatus() const;
};

#endif