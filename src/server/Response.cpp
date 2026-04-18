#include "Server.hpp"
#include "Cgi.hpp"
#include <sstream>
#include <dirent.h>


// mime types
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
	if (ext == ".ico") return "image/x-icon";
	if (ext == ".svg") return "image/svg+xml";
	if (ext == ".mp4") return "video/mp4";
	if (ext == ".pdf") return "application/pdf";
	if (ext == ".txt") return "text/plain";
	return "application/octet-stream";
}

// routing (longest prefix wins)

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


// from the uri you type to the filesystem one the path that the server will use localy
static	std::string resolvePath(const std::string& uri, LocationConfig* loc) {
	std::string root = loc->root;
	if (!root.empty() && root[root.size()-1] == '/')
		root.erase(root.size()-1);
	std::string remain = uri.substr(loc->path.size());
	if (remain.empty() || remain[0] != '/')
		remain = "/" + remain;
	return root + remain;
}

static std::string pathWithoutQuery(const std::string& uri) {
	size_t q = uri.find('?');
	if (q == std::string::npos) return uri;
	return uri.substr(0, q);
}

static std::string getExtension(const std::string& path) {
	size_t slash = path.rfind('/');
	size_t dot = path.rfind('.');
	if (dot == std::string::npos) return "";
	if (slash != std::string::npos && dot < slash) return "";
	return path.substr(dot);
}

static	std::string buildAutoindex(const std::string& uri, const std::string& dir_path) {
	std::ostringstream html;
	html << "<!DOCKTYPE html>\n<html><head><title>Index of " << uri << "</title></head>\n"
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

// --- POST: FILE UPLOAD ---
static bool saveUpload(Client& c, LocationConfig* loc) {
    if (loc->upload_store.empty()) return false;

    // Try to get filename from Content-Disposition header
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
    // Fall back to timestamp-based name
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

// --- SERVER SELECTION ---
// Picks the right ServerConfig based on which listen fd accepted the connection.
// Falls back to the first config if no match found.
static ServerConfig& selectServer(std::vector<ServerConfig>& configs,
                                   const std::map<int, int>& fd_to_config,
                                   int listen_fd) {
    std::map<int, int>::const_iterator it = fd_to_config.find(listen_fd);
    if (it != fd_to_config.end())
        return configs[it->second];
    return configs[0];
}

// --- BUILD RESPONSE ---
void Server::buildResponse(Client& c) {
    ServerConfig& srv = selectServer(_configs, _fd_to_config, c.getListenFd());

    // Early exit for errors set during request parsing (400, 413)
    if (c.getErrorCode() != 0) {
        std::string msg = (c.getErrorCode() == 413) ? "Content Too Large" : "Bad Request";
        c.sendBuf() = buildErrorResponse(c.getErrorCode(), msg, srv);
        c.setFileSize(c.sendBuf().size());
        return;
    }

    std::string request_uri = c.getPath();
    std::string uri_path = pathWithoutQuery(request_uri);
    LocationConfig* loc = matchLocation(srv, uri_path);

    // --- 1. Redirect ---
    if (loc && loc->return_url.first != 0) {
        std::ostringstream oss;
        int code = loc->return_url.first;
        oss << "HTTP/1.1 " << code << " Moved\r\n"
            << "Location: " << loc->return_url.second << "\r\n"
            << "Content-Length: 0\r\n"
            << "Connection: close\r\n\r\n";
        c.sendBuf() = oss.str();
        c.setFileSize(c.sendBuf().size());
        std::cout << "[RESPONSE] " << code << " -> " << loc->return_url.second << std::endl;
        return;
    }

    // --- 2. Method validation ---
    if (loc && !loc->allowed_methods.empty()) {
        bool allowed = false;
        for (size_t i = 0; i < loc->allowed_methods.size(); ++i)
            if (loc->allowed_methods[i] == c.getMethod()) { allowed = true; break; }
        if (!allowed) {
            c.sendBuf() = buildErrorResponse(405, "Method Not Allowed", srv);
            c.setFileSize(c.sendBuf().size());
            std::cout << "[RESPONSE] 405: " << c.getMethod() << " not allowed" << std::endl;
            return;
        }
    }

    // No location matched and no root configured -> 404
    if (!loc || loc->root.empty()) {
        c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
        c.setFileSize(c.sendBuf().size());
        return;
    }

    std::string physical = resolvePath(uri_path, loc);
    std::cout << "[ROUTE] " << c.getMethod() << " " << request_uri
              << " -> " << physical << std::endl;

    // --- 3. CGI ---
    std::string ext = getExtension(uri_path);
    std::map<std::string, std::string>::const_iterator cgi_it = srv.cgi_handlers.find(ext);
    if (cgi_it != srv.cgi_handlers.end()) {
        struct stat cgi_st;
        if (stat(physical.c_str(), &cgi_st) != 0 || !S_ISREG(cgi_st.st_mode)) {
            c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
            c.setFileSize(c.sendBuf().size());
            return;
        }

        size_t slash = physical.rfind('/');
        std::string script_dir = (slash == std::string::npos) ? "." : physical.substr(0, slash);
        if (access(cgi_it->second.c_str(), X_OK) != 0 ||
            access(physical.c_str(), R_OK) != 0 ||
            access(script_dir.c_str(), X_OK) != 0) {
            c.sendBuf() = buildErrorResponse(403, "Forbidden", srv);
            c.setFileSize(c.sendBuf().size());
            return;
        }

        CgiHandler cgi;
        if (!cgi.execute(c, srv, request_uri, physical, cgi_it->second)) {
            c.sendBuf() = buildErrorResponse(500, "Internal Server Error", srv);
            c.setFileSize(c.sendBuf().size());
            return;
        }

        c.sendBuf() = cgi.getResponse();
        c.setFileSize(c.sendBuf().size());
        return;
    }

    // --- 4. DELETE ---
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
            std::cout << "[RESPONSE] 204 Deleted: " << physical << std::endl;
        }
        c.setFileSize(c.sendBuf().size());
        return;
    }

    // --- 5. POST (file upload) ---
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
                std::cout << "[RESPONSE] 201 Created" << std::endl;
            } else {
                c.sendBuf() = buildErrorResponse(500, "Internal Server Error", srv);
            }
        } else {
            // POST to a route without upload_store -> 204
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

    // --- 6. GET ---
    struct stat st;

    if (stat(physical.c_str(), &st) != 0) {
        c.sendBuf() = buildErrorResponse(404, "Not Found", srv);
        c.setFileSize(c.sendBuf().size());
        std::cout << "[RESPONSE] 404: " << physical << std::endl;
        return;
    }

    // Directory handling
    if (S_ISDIR(st.st_mode)) {
        // Missing trailing slash -> redirect
        if (uri_path[uri_path.size()-1] != '/') {
            std::ostringstream oss;
            oss << "HTTP/1.1 301 Moved Permanently\r\n"
                << "Location: " << uri_path << "/\r\n"
                << "Content-Length: 0\r\n"
                << "Connection: close\r\n\r\n";
            c.sendBuf() = oss.str();
            c.setFileSize(c.sendBuf().size());
            return;
        }

        // Try index file
        if (!loc->index.empty()) {
            std::string index_path = physical;
            if (index_path[index_path.size()-1] != '/') index_path += "/";
            index_path += loc->index;
            struct stat ist;
            if (stat(index_path.c_str(), &ist) == 0 && S_ISREG(ist.st_mode)) {
                physical = index_path;
                st = ist;
                goto serve_file; // found index, fall through to file serving
            }
        }

        // Autoindex
        if (loc->autoindex) {
            std::string body = buildAutoindex(uri_path, physical);
            std::ostringstream oss;
            oss << "HTTP/1.1 200 OK\r\n"
                << "Server: Webserv/1.0\r\n"
                << "Content-Type: text/html\r\n"
                << "Content-Length: " << body.size() << "\r\n"
                << "Connection: " << (c.isKeepAlive() ? "keep-alive" : "close") << "\r\n\r\n"
                << body;
            c.sendBuf() = oss.str();
            c.setFileSize(c.sendBuf().size());
            std::cout << "[RESPONSE] 200 Autoindex" << std::endl;
            return;
        }

        // Directory but no index and no autoindex -> 403
        c.sendBuf() = buildErrorResponse(403, "Forbidden", srv);
        c.setFileSize(c.sendBuf().size());
        return;
    }

serve_file:
    // Regular file
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

    // For file responses, file_size is the actual file size.
    // bytes_sent tracks file bytes only (header is separate via sendBuf).
    c.setFileSize(st.st_size);
    {
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Server: Webserv/1.0\r\n"
            << "Content-Length: " << st.st_size << "\r\n"
            << "Content-Type: " << getMimeType(physical) << "\r\n"
            << "Connection: " << (c.isKeepAlive() ? "keep-alive" : "close") << "\r\n\r\n";
        c.sendBuf() = oss.str();
    }
    std::cout << "[RESPONSE] 200 OK: " << physical << std::endl;
}

// --- SEND RESPONSE ---
// Two-phase send: first drain sendBuf (headers + small inline bodies),
// then stream the file if one is open.
// bytes_sent ONLY counts file bytes — it is never incremented during header sending.
// This fixes the previous bug where inline responses never triggered "done".
void Server::handleResponse(int fd) {
    Client& c = *clients[fd];
    char chunk[8192];

    // Phase 1: send headers (and full body for non-file responses)
    if (!c.headerSent()) {
        ssize_t n = send(fd, c.sendBuf().c_str(), c.sendBuf().size(), 0);
        if (n > 0) {
            c.sendBuf().erase(0, n);
            if (c.sendBuf().empty())
                c.setHeaderSent(true);
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return; // epoll will call us again when writable
        } else {
            c.setState(CLOSED);
            return;
        }
        // Still have header bytes to send
        if (!c.headerSent()) return;
    }

    // Phase 2: stream file body (only for real file responses)
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
            return; // always return here; check done on next event
        }
        // File fully sent — fall through to done logic
    }

    // Phase 3: done
    // For inline responses: sendBuf is empty and no file -> done immediately.
    // For file responses: bytes_sent >= file_size -> done.
    bool file_done = !c.file_stream.is_open() ||
                     (c.getBytesSent() >= c.getFileSize());

    if (c.sendBuf().empty() && file_done) {
        std::cout << "[DONE] fd=" << fd
                  << (c.isKeepAlive() ? " keep-alive" : " close") << std::endl;
        if (c.isKeepAlive()) {
            c.reset();
            modifyEpoll(fd, EPOLLIN);
        } else {
            c.setState(CLOSED);
        }
    }
}
