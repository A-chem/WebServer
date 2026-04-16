#include "Server.hpp"
#include <sstream>
#include <cctype>

// normalize the path (hna y9dr ikon path traversal o tssd9 mkhli server yfot root so each .. ghadi nrj3o lor 7ta nwsslo root o n7bsso)

static	std::string normalizePath(const std::string& path) {
	if (path.empty() || path[0] != '/') return "";

	std::vector<std::string> segments;
	std::istringstream ss(path);
	std::string seg;

	while (std::getline(ss, seg, '/')) {
		if (seg.empty() || seg == ".") continue;
		if (seg == "..") {
			if (!segments.empty()) segments.pop_back();
			else return "";
		} else {
			segments.push_back(seg);
		}
	}
	std::string result = "/";
	for (size_t i = 0; i < segments.size(); i++) {
		if (i > 0) result += "/";
		result += segments[i];
	}
	if (path[path.size() - 1] == '/' && result != "/")
		result += "/";
	return result;
}

// request line (awel line mn  request )

static	size_t parseRequestLine(Client& c) {
	size_t crlf = c.recvBuf().find("\r\n");
	if (crlf == std::string::npos) return std::string::npos;

	std::string line = c.recvBuf().substr(0, crlf);

	size_t s1 = line.find(' ');
	if (s1 == std::string::npos) return std::string::npos;
	size_t s2 = line.find(' ', s1 + 1);
	if (s2 == std::string::npos) return std::string::npos;

	std::string method = line.substr(0, s1);
	std::string rawpath = line.substr(s1 + 1, s2 - s1 - 1);
	std::string version = line.substr(s2 + 1);

	if (!version.empty() && version[version.size() - 1] == '\r')
		version.erase(version.size() - 1);

	std::string path = rawpath;
	std::string query;
	size_t qmark = rawpath.find('?');
	if (qmark != std::string::npos) {
		path = rawpath.substr(0, qmark);
		query = rawpath.substr(qmark);
	}

	std::string safe = normalizePath(path);
	if (safe.empty()) return std::string::npos;

	c.setMethod(method);
	c.setPath(safe + query);
	c.setVersion(version);
	return crlf + 2;
}

// parse the headers

static void parseHeaders(Client& c) {
	std::string& buf = c.recvBuf();
	size_t end = buf.find("\r\n\r\n");
	if (end == std::string::npos) return ;

	std::istringstream ss(buf.substr(0, end));
	std::string line;

	while (std::getline(ss, line)) {
		if (line == "\r"  || line.empty()) continue;
		size_t colon = line.find(':');
		if (colon == std::string::npos) continue;

		std::string key = line.substr(0, colon);
		std::string value = line.substr(colon + 1);

		size_t start = value.find_first_not_of(" \t");
		if (start != std::string::npos) value = value.substr(start);

		if (!value.empty() && value[value.size() - 1] == '\r')
			value.erase(value.size() - 1);

		c.setHeader(key, value);
	}
}

static std::string toLowerCopy(const std::string& s) {
	std::string out = s;
	for (size_t i = 0; i < out.size(); ++i)
		out[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(out[i])));
	return out;
}

static bool hasChunkedEncoding(const std::map<std::string, std::string>& hdrs) {
	std::map<std::string, std::string>::const_iterator it = hdrs.find("Transfer-Encoding");
	if (it == hdrs.end())
		return false;
	return toLowerCopy(it->second).find("chunked") != std::string::npos;
}

static bool decodeChunkedBody(const std::string& in, std::string& out, size_t& consumed) {
	out.clear();
	consumed = 0;
	size_t pos = 0;

	while (true) {
		size_t lineEnd = in.find("\r\n", pos);
		if (lineEnd == std::string::npos) return false;
		std::string sizeLine = in.substr(pos, lineEnd - pos);
		size_t semi = sizeLine.find(';');
		if (semi != std::string::npos)
			sizeLine = sizeLine.substr(0, semi);
		if (sizeLine.empty()) return false;
		for (size_t i = 0; i < sizeLine.size(); ++i) {
			if (!std::isxdigit(static_cast<unsigned char>(sizeLine[i])))
				return false;
		}

		size_t chunkSize = static_cast<size_t>(std::strtoul(sizeLine.c_str(), NULL, 16));
		pos = lineEnd + 2;
		if (in.size() < pos + chunkSize + 2) return false;
		out.append(in, pos, chunkSize);
		pos += chunkSize;
		if (in.compare(pos, 2, "\r\n") != 0) return false;
		pos += 2;

		if (chunkSize == 0) {
			if (in.size() < pos + 2) return false;
			if (in.compare(pos, 2, "\r\n") != 0) {
				size_t trailerEnd = in.find("\r\n\r\n", pos);
				if (trailerEnd == std::string::npos) return false;
				pos = trailerEnd + 4;
			}
			else {
				pos += 2;
			}
			consumed = pos;
			return true;
		}
	}
}

// handle request 

void	Server::handleRequest(int fd) {
	Client& c = *clients[fd];
	char buf[4069];

	ssize_t n = recv(fd, buf, sizeof(buf), 0);
	if (n <= 0) {
		if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
			c.setState(CLOSED);
		return ;
	}
	c.recvBuf().append(buf, n);

	while (true) {
		if (c.getState() == READ_REQUEST_LINE) {
			if (c.recvBuf().find("\r\n") == std::string::npos) break;

			size_t consumed = parseRequestLine(c);
			if (consumed == std::string::npos) {
				c.setErrorCode(400);
				c.setState(PROCESS_REQUEST);
				break;
			}
			c.recvBuf().erase(0, consumed); // mss7 consumed part 
			c.setState(READ_REQUEST_HEADER);
		}
		else if (c.getState() == READ_REQUEST_HEADER) {
			size_t pos = c.recvBuf().find("\r\n\r\n");
			if (pos == std::string::npos) break;
			parseHeaders(c);
			c.recvBuf().erase(0, pos + 4);

			// keep-alive : ida kan HTTP/1.1 rah default hiya n resusiw nfs client ila ida kant connetion:close
			std::map<std::string, std::string> hdrs = c.getHeader();
			std::string conn = hdrs.count("Connection") ? hdrs["Connection"] : "";
			if (c.getVersion() == "HTTP/1.1" && conn != "close")
				c.setKeepAlive(true);
			else if (conn == "keep-alive")
				c.setKeepAlive(true);
			else
				c.setKeepAlive(false);

			if (hasChunkedEncoding(hdrs)) {
				c.setContentLength(0);
				c.setState(READ_BODY);
			}
			else if (hdrs.count("Content-Length")) {
				size_t cl = static_cast<size_t>(std::atoi(hdrs["Content-Length"].c_str()));
				c.setContentLength(cl);

				ServerConfig& srv = _configs[0];
				size_t max_body = srv.client_max_body_size;
				LocationConfig* loc = NULL;
				size_t longest = 0;
				for (size_t i = 0; i < srv.locations.size(); i++) {
					const std::string& lp = srv.locations[i].path;
					if (c.getPath().find(lp) == 0 && lp.size() > longest) {
						size_t end_idx = lp.size();
						if (end_idx == c.getPath().size() || 
								c.getPath()[end_idx] == '/' || 
								lp[lp.size()-1] == '/') {
							longest = lp.size();
							loc = &srv.locations[i];
						}
					}
				}
				if (loc && loc->client_max_body_size > 0)
					max_body = loc->client_max_body_size;
				if (max_body > 0 && cl > max_body) {
					c.setErrorCode(413);
					c.setState(PROCESS_REQUEST);
					break;
				}

				if (cl > 0)
					c.setState(READ_BODY);
				else
					c.setState(PROCESS_REQUEST);
			} else {
				c.setContentLength(0);
				c.setState(PROCESS_REQUEST);
			}
		}
		else if (c.getState() == READ_BODY) {
			std::map<std::string, std::string> hdrs = c.getHeader();
			if (hasChunkedEncoding(hdrs)) {
				std::string decoded;
				size_t consumed = 0;
				if (!decodeChunkedBody(c.recvBuf(), decoded, consumed))
					break;
				c.setBody(decoded);
				c.setContentLength(decoded.size());
				c.recvBuf().erase(0, consumed);
				c.setState(PROCESS_REQUEST);
			}
			else if (c.recvBuf().size() >= c.getContentLength()) {
				c.setBody(c.recvBuf().substr(0, c.getContentLength()));
				c.recvBuf().erase(0, c.getContentLength());
				c.setState(PROCESS_REQUEST);
			}
			else {
				break;
			}
		}
		else if (c.getState() == PROCESS_REQUEST) {
			buildResponse(c);
			c.setState(WRITE_RESPONSE);
			modifyEpoll(fd, EPOLLOUT);
			break;
		}
		else { break; }
	}
}
