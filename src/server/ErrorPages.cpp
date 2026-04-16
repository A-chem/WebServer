#include "Server.hpp"

std::string Server::defaultErrorPage(int code, const std::string& msg) {
	std::ostringstream oss;
	oss << "<!DOCTYPE html>\r\n"
		<< "<html><head><title>" << code << " " << msg << "</title></head>\r\n"
		<< "<body>\r\n"
		<< "<h1>" << code << " " << msg << "</h1>\r\n"
		<< "<hr><p>Webserv/1.0</p>\r\n"
		<< "</body></html>\r\n";
	return oss.str();
}

std::string Server::buildErrorResponse(int code, const std::string& msg, ServerConfig& srv) {
	std::string body;

	if (srv.error_pages.count(code)) {
		std::string page_path = srv.error_pages[code];
		std::string full_path;
		if (!srv.locations.empty())
			full_path = srv.locations[0].root + page_path;
		else
			full_path = page_path;
		std::ifstream f(full_path.c_str(), std::ios::binary);
		if (f.is_open()) {
			std::ostringstream ss;
			ss << f.rdbuf();
			body = ss.str();
		}
	}
	if (body.empty())
		body = defaultErrorPage(code, msg);

	std::ostringstream response;
	response << "HTTP/1.1" << code << " " << msg << "\r\n"
		<< "Server: Webserv/1.0\r\n"
		<< "Content-Length: " << body.size() << "\r\n"
		<< "Connection: close\r\n"
		<< "\r\n"
		<< body;
	return response.str();
}
