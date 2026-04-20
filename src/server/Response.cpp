#include "Server.hpp"
#include "CGI.hpp"
#include <sstream>
#include <dirent.h>

static std::string getMimeType(const std::string& path) {
	size_t dot = path.rfind('.');
	if (dot == std::string::npos) return "application/octet-stream";
	std::string ext = path.substr(dot);
	if (ext == ".html" || ext == ".htm") return "text/html";
	if (ext == ".css") return "text/css";
	if (ext == ".js") return "application/javascript";
	if (ext == ".json") return "application/json";
	if (ext == ".png") return "image/png";
	if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
	if (ext == ".gif") return "image/gif";
	if (ext == ".txt") return "text/plain";
	return "application/octet-stream";
}

static LocationConfig* matchLocation(ServerConfig& srv, const std::string& uri) {
	LocationConfig* best = NULL;
	size_t longest = 0;

	for (size_t i = 0; i < srv.locations.size(); i++) {
		const std::string& lp = srv.locations[i].path;
		if (uri.find(lp) != 0) continue;
		size_t end_idx = lp.size();
		if (end_idx != uri.size() && uri[end_idx] != '/' && lp[lp.size() - 1] != '/')
			continue;
		if (lp.size() > longest) {
			longest = lp.size();
			best = &srv.locations[i];
		}
	}
	return best;
}

static std::string resolvePath(const std::string& uri, LocationConfig* loc)
{
    std::string root = loc->root;

    if (!root.empty() && root[root.size() - 1] == '/')
        root.erase(root.size() - 1);

    // STEP 1: remove location prefix
    std::string remain = uri.substr(loc->path.size());

    // STEP 2: fix slash
    if (!remain.empty() && remain[0] != '/')
        remain = "/" + remain;

    // STEP 3: IMPORTANT FIX
    // rebuild full path INCLUDING location directory name
    std::string locationPart = loc->path;

    // remove trailing slash from location
    if (!locationPart.empty() && locationPart[locationPart.size() - 1] == '/')
        locationPart.erase(locationPart.size() - 1);

    std::string result = root + locationPart + remain;

    std::cout << "[RESOLVE] URI: " << uri << std::endl;
    std::cout << "[RESOLVE] Location: " << loc->path << std::endl;
    std::cout << "[RESOLVE] Remain: " << remain << std::endl;
    std::cout << "[RESOLVE] Result: " << result << std::endl;

    return result;
}

static std::string buildAutoindex(const std::string& uri, const std::string& dir_path) {
	std::ostringstream html;
	html << "<!DOCTYPE html>\n<html><head><title>Index of " << uri << "</title></head>\n"
		<< "<body><h1>Index of " << uri << "</h1><hr><pre>\n";

	DIR* dir = opendir(dir_path.c_str());
	if (dir) {
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			std::string name = entry->d_name;
			if (name == ".") continue;
			bool is_dir = (entry->d_type == DT_DIR);
			html << "<a href=\"" << name << (is_dir ? "/" : "") << "\">"
				<< name << (is_dir ? "/" : "") << "</a>\n";
		}
		closedir(dir);
	}
	html << "</pre><hr></body></html>\n";
	return html.str();
}

static ServerConfig& selectServer(std::vector<ServerConfig>& configs,
                                   const std::map<int, int>& fd_to_config,
                                   int listen_fd) {
	std::map<int, int>::const_iterator it = fd_to_config.find(listen_fd);
	if (it != fd_to_config.end())
		return configs[it->second];
	return configs[0];
}

static bool saveUpload(Client& c, LocationConfig* loc) {
	if (loc->upload_store.empty()) return false;

	std::string filename;
	std::map<std::string, std::string> hdrs = c.getHeader();
	if (hdrs.count("Content-Disposition")) {
		std::string cd = hdrs["Content-Disposition"];
		size_t fn = cd.find("filename=\"");
		if (fn != std::string::npos) {
			fn += 10;
			size_t fe = cd.find("\"", fn);
			if (fe != std::string::npos)
				filename = cd.substr(fn, fe - fn);
		}
	}
	if (filename.empty()) {
		std::ostringstream ss;
		ss << "upload_" << (size_t)time(NULL) << ".bin";
		filename = ss.str();
	}

	std::string store = loc->upload_store;
	if (store[store.size()-1] != '/') store += "/";

	std::ofstream out((store + filename).c_str(), std::ios::binary);
	if (!out.is_open()) return false;
	out.write(c.getBody().c_str(), c.getBody().size());
	return out.good();
}

static const char* getStatusMsg(int code) {
	if (code == 200) return "OK";
	if (code == 201) return "Created";
	if (code == 204) return "No Content";
	if (code == 301) return "Moved Permanently";
	if (code == 403) return "Forbidden";
	if (code == 404) return "Not Found";
	if (code == 405) return "Method Not Allowed";
	if (code == 500) return "Internal Server Error";
	if (code == 504) return "Gateway Timeout";
	return "Unknown";
}

void Server::buildResponse(Client& c) {
	ServerConfig& srv = selectServer(_configs, _fd_to_config, c.getListenFd());

	if (c.getErrorCode() != 0) {
		std::string msg = (c.getErrorCode() == 413) ? "Content Too Large" : "Bad Request";
		c.sendBuf() = buildErrorResponse(c.getErrorCode(), msg, srv);
		c.setFileSize(c.sendBuf().size());
		return;
	}

	LocationConfig* loc = matchLocation(srv, c.getPath());

	if (loc && loc->return_url.first != 0) {
		std::ostringstream oss;
		int code = loc->return_url.first;
		oss << "HTTP/1.1 " << code << " Moved\r\n"
			<< "Location: " << loc->return_url.second << "\r\n"
			<< "Content-Length: 0\r\n"
			<< "Connection: close\r\n\r\n";
		c.sendBuf() = oss.str();
		c.setFileSize(c.sendBuf().size());
		return;
	}

	if (loc && !loc->allowed_methods.empty()) {
		bool allowed = false;
		for (size_t i = 0; i < loc->allowed_methods.size(); ++i)
			if (loc->allowed_methods[i] == c.getMethod()) { allowed = true; break; }
		if (!allowed) {
			c.sendBuf() = buildErrorResponse(405, "Method Not Allowed", srv);
			c.setFileSize(c.sendBuf().size());
			return;
		}
	}

	if (!loc || loc->root.empty()) {
		c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
		c.setFileSize(c.sendBuf().size());
		return;
	}

	std::string physical = resolvePath(c.getPath(), loc);

	std::cout << "[ROUTE] URI: " << c.getPath() << std::endl;
	std::cout << "[ROUTE] Location: " << loc->path << std::endl;
	std::cout << "[ROUTE] Root: " << loc->root << std::endl;
	std::cout << "[ROUTE] Physical: " << physical << std::endl;

	// === CGI HANDLING ===
	if (!loc->cgi_pass.empty()) {

		size_t dot = physical.rfind('.');
        				std::cout << "[CGI] Interpreter: ssssssssssssssss " << dot << std::endl;

		if (dot != std::string::npos) {
			std::string ext = physical.substr(dot);
			
			for (std::map<std::string, std::string>::iterator it = loc->cgi_pass.begin(); 
         it != loc->cgi_pass.end(); ++it) {
        std::cout << "Key (Extension): [" << it->first << "] -> Value (Interpreter): [" << it->second << "]" << std::endl;
    }
			
			if (loc->cgi_pass.find(ext) != loc->cgi_pass.end()) {
				std::string interpreter = loc->cgi_pass[ext];
				
				std::cout << "\n[CGI] === START ===" << std::endl;
				std::cout << "[CGI] Interpreter: " << interpreter << std::endl;
				std::cout << "[CGI] Script: " << physical << std::endl;
				
				// Check if script exists
				if (access(physical.c_str(), F_OK) != 0) {
					std::cout << "[CGI] ERROR: Script not found" << std::endl;
					c.sendBuf() = buildErrorResponse(404, "Script Not Found", srv);
					c.setFileSize(c.sendBuf().size());
					return;
				}
				
				if (access(physical.c_str(), X_OK) != 0) {
                    std::cout << "hhhhhhhhh" << physical << std::endl;
					std::cout << "[CGI] 2ERROR: Script not executable" << std::endl;
					c.sendBuf() = buildErrorResponse(403, "Script Not Executable", srv);
					c.setFileSize(c.sendBuf().size());
					return;
				}
				
				size_t qmark = c.getPath().find('?');
				std::string query = (qmark != std::string::npos) ?
					c.getPath().substr(qmark + 1) : "";
				
				CGI cgi;
				cgi.setMethod(c.getMethod());
				cgi.setPath(physical);
				cgi.setQuery(query);
				cgi.setBody(c.getBody());
				cgi.setContentType(c.getHeader().count("Content-Type") ?
					c.getHeader()["Content-Type"] : "");
				cgi.setHost(c.getHeader().count("Host") ?
					c.getHeader()["Host"] : "localhost");
				
				int result = cgi.execute(interpreter, 30);
				
				std::cout << "[CGI] Result: " << result << std::endl;
				
				if (cgi.hasError()) {
					std::cout << "[CGI] Has error, returning error response" << std::endl;
					c.sendBuf() = buildErrorResponse(result, getStatusMsg(result), srv);
					c.setFileSize(c.sendBuf().size());
					return;
				}
				
				int status = cgi.getStatus();
				std::string body = cgi.getBody();
				
				std::cout << "[CGI] Status: " << status << std::endl;
				std::cout << "[CGI] Body size: " << body.size() << std::endl;
				
				std::ostringstream oss;
				oss << "HTTP/1.1 " << status << " " << getStatusMsg(status) << "\r\n"
					<< "Server: Webserv/1.0\r\n"
					<< "Content-Length: " << body.size() << "\r\n"
					<< "Content-Type: text/html\r\n"
					<< "Connection: close\r\n\r\n";
				
				if (!body.empty())
					oss << body;
				
				c.sendBuf() = oss.str();
				c.setFileSize(c.sendBuf().size());
				
				std::cout << "[CGI] === COMPLETE ===" << std::endl << std::endl;
				return;
			}
		}
	}

	// === DELETE ===
	if (c.getMethod() == "DELETE") {
		struct stat st;
		if (physical.empty() || stat(physical.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
			c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
		} else if (remove(physical.c_str()) != 0) {
			c.sendBuf() = buildErrorResponse(403, "Forbidden", srv);
		} else {
			std::ostringstream oss;
			oss << "HTTP/1.1 204 No Content\r\n"
				<< "Server: Webserv/1.0\r\n"
				<< "Content-Length: 0\r\n"
				<< "Connection: close\r\n\r\n";
			c.sendBuf() = oss.str();
		}
		c.setFileSize(c.sendBuf().size());
		return;
	}

	// === POST ===
	if (c.getMethod() == "POST") {
		if (loc && !loc->upload_store.empty()) {
			if (saveUpload(c, loc)) {
				std::string body = "<html><body><h1>201 Created</h1></body></html>";
				std::ostringstream oss;
				oss << "HTTP/1.1 201 Created\r\n"
					<< "Server: Webserv/1.0\r\n"
					<< "Content-Length: " << body.size() << "\r\n"
					<< "Content-Type: text/html\r\n"
					<< "Connection: close\r\n\r\n" << body;
				c.sendBuf() = oss.str();
			} else {
				c.sendBuf() = buildErrorResponse(500, "Internal Server Error", srv);
			}
		} else {
			std::ostringstream oss;
			oss << "HTTP/1.1 204 No Content\r\n"
				<< "Server: Webserv/1.0\r\n"
				<< "Content-Length: 0\r\n"
				<< "Connection: close\r\n\r\n";
			c.sendBuf() = oss.str();
		}
		c.setFileSize(c.sendBuf().size());
		return;
	}

	// === GET ===
	struct stat st;

	if (stat(physical.c_str(), &st) != 0) {
		c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
		c.setFileSize(c.sendBuf().size());
		return;
	}

	if (S_ISDIR(st.st_mode)) {
		if (c.getPath()[c.getPath().size()-1] != '/') {
			std::ostringstream oss;
			oss << "HTTP/1.1 301 Moved Permanently\r\n"
				<< "Location: " << c.getPath() << "/\r\n"
				<< "Content-Length: 0\r\n"
				<< "Connection: close\r\n\r\n";
			c.sendBuf() = oss.str();
			c.setFileSize(c.sendBuf().size());
			return;
		}

		if (!loc->index.empty()) {
			std::string index_path = physical;
			if (index_path[index_path.size()-1] != '/') index_path += "/";
			index_path += loc->index;
			struct stat ist;
			if (stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
				physical = index_path;
				st = ist;
				goto serve_file;
			}
		}

		if (loc->autoindex) {
			std::string body = buildAutoindex(c.getPath(), physical);
			std::ostringstream oss;
			oss << "HTTP/1.1 200 OK\r\n"
				<< "Server: Webserv/1.0\r\n"
				<< "Content-Type: text/html\r\n"
				<< "Content-Length: " << body.size() << "\r\n"
				<< "Connection: close\r\n\r\n"
				<< body;
			c.sendBuf() = oss.str();
			c.setFileSize(c.sendBuf().size());
			return;
		}

		c.sendBuf() = buildErrorResponse(403, "Forbidden", srv);
		c.setFileSize(c.sendBuf().size());
		return;
	}

serve_file:
	if (!S_ISREG(st.st_mode)) {
		c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
		c.setFileSize(c.sendBuf().size());
		return;
	}

	c.openFile(physical);
	if (!c.file_stream.is_open()) {
		c.sendBuf() = buildErrorResponse(403, "Forbidden", srv);
		c.setFileSize(c.sendBuf().size());
		return;
	}

	c.setFileSize(st.st_size);
	{
		std::ostringstream oss;
		oss << "HTTP/1.1 200 OK\r\n"
			<< "Server: Webserv/1.0\r\n"
			<< "Content-Length: " << st.st_size << "\r\n"
			<< "Content-Type: " << getMimeType(physical) << "\r\n"
			<< "Connection: close\r\n\r\n";
		c.sendBuf() = oss.str();
	}
}

void Server::handleResponse(int fd) {
	Client& c = *clients[fd];
	char chunk[8192];

	if (!c.headerSent()) {
		ssize_t n = send(fd, c.sendBuf().c_str(), c.sendBuf().size(), 0);
		if (n > 0) {
			c.sendBuf().erase(0, n);
			if (c.sendBuf().empty())
				c.setHeaderSent(true);
		} else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
			return;
		} else {
			c.setState(CLOSED);
			return;
		}
		if (!c.headerSent()) return;
	}

	if (c.file_stream.is_open()) {
		if (c.getBytesSent() < c.getFileSize()) {
			c.file_stream.clear();
			c.file_stream.seekg(static_cast<std::streamoff>(c.getBytesSent()));

			std::streamsize actual = c.readFile(chunk, sizeof(chunk));
			if (actual <= 0) { c.setState(CLOSED); return; }

			ssize_t n = send(fd, chunk, static_cast<size_t>(actual), 0);
			if (n == -1) {
				if (errno == EAGAIN || errno == EWOULDBLOCK) return;
				c.setState(CLOSED);
				return;
			}
			if (n > 0) c.setBytesSent(static_cast<size_t>(n));
			return;
		}
	}

	bool file_done = !c.file_stream.is_open() ||
	                 (c.getBytesSent() >= c.getFileSize());

	if (c.sendBuf().empty() && file_done) {
		if (c.isKeepAlive()) {
			c.reset();
			modifyEpoll(fd, EPOLLIN);
		} else {
			c.setState(CLOSED);
		}
	}
}