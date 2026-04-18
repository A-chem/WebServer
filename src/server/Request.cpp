#include "Server.hpp"
#include <cctype>
#include <sstream>
#include <cstdlib>

static const size_t kMaxChunkedBodyBuffer = 50 * 1024 * 1024;

static std::string getHeaderValue(const std::map<std::string, std::string>& headers, const std::string& key) {
	for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
		if (it->first.size() != key.size())
			continue;
		bool same = true;
		for (size_t i = 0; i < key.size(); ++i) {
			if (std::tolower(static_cast<unsigned char>(it->first[i])) !=
				std::tolower(static_cast<unsigned char>(key[i]))) {
				same = false;
				break;
			}
		}
		if (same)
			return it->second;
	}
	return "";
}

static bool isChunkedTransfer(const std::string& value) {
	std::string lowered = value;
	for (size_t i = 0; i < lowered.size(); ++i)
		lowered[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lowered[i])));
	return lowered.find("chunked") != std::string::npos;
}

static bool parseChunkSize(const std::string& line, size_t& sizeOut) {
	std::string chunkLine = line;
	size_t semicolon = chunkLine.find(';');
	if (semicolon != std::string::npos)
		chunkLine = chunkLine.substr(0, semicolon);
	if (chunkLine.empty())
		return false;
	std::istringstream ss(chunkLine);
	ss >> std::hex >> sizeOut;
	return !ss.fail();
}

static bool decodeChunkedBody(const std::string& raw, std::string& bodyOut, size_t& consumed) {
	size_t pos = 0;
	bodyOut.clear();
	consumed = 0;
	while (true) {
		size_t lineEnd = raw.find("\r\n", pos);
		if (lineEnd == std::string::npos)
			return false;
		size_t chunkSize = 0;
		if (!parseChunkSize(raw.substr(pos, lineEnd - pos), chunkSize))
			return false;
		pos = lineEnd + 2;
		if (raw.size() < pos + chunkSize + 2)
			return false;
		if (chunkSize > 0)
			bodyOut.append(raw.substr(pos, chunkSize));
		pos += chunkSize;
		if (raw.substr(pos, 2) != "\r\n")
			return false;
		pos += 2;
		if (chunkSize == 0) {
			if (pos == raw.size()) {
				consumed = pos;
				return true;
			}
			size_t trailerEnd = raw.find("\r\n\r\n", pos);
			if (trailerEnd != std::string::npos) {
				consumed = trailerEnd + 4;
				return true;
			}
			if (raw.size() >= pos + 2 && raw.substr(pos, 2) == "\r\n") {
				consumed = pos + 2;
				return true;
			}
			return false;
		}
	}
}

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
		query = rawpath.substr(qmark + 1);
	}

	std::string safe = normalizePath(path);
	if (safe.empty()) return std::string::npos;

	c.setMethod(method);
	c.setPath(safe);
	c.setQueryString(query);
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
			std::string conn = getHeaderValue(hdrs, "Connection");
			if (c.getVersion() == "HTTP/1.1" && conn != "close")
				c.setKeepAlive(true);
			else if (conn == "keep-alive")
				c.setKeepAlive(true);
			else
				c.setKeepAlive(false);

			std::string te = getHeaderValue(hdrs, "Transfer-Encoding");
			if (!te.empty() && isChunkedTransfer(te)) {
				c.setChunkedBody(true);
				c.setState(READ_BODY);
				continue;
			}

			std::string clHeader = getHeaderValue(hdrs, "Content-Length");
			if (!clHeader.empty()) {
				size_t cl = static_cast<size_t>(std::atoi(clHeader.c_str()));
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
				c.setChunkedBody(false);
				c.setState(PROCESS_REQUEST);
			}
		}
		else if (c.getState() == READ_BODY) {
			if (c.isChunkedBody()) {
				std::string decoded;
				size_t consumed = 0;
				if (decodeChunkedBody(c.recvBuf(), decoded, consumed)) {
					c.setBody(decoded);
					c.recvBuf().erase(0, consumed);
					c.setChunkedBody(false);
					c.setState(PROCESS_REQUEST);
				} else {
					if (c.recvBuf().size() > kMaxChunkedBodyBuffer) {
						c.setErrorCode(400);
						c.setState(PROCESS_REQUEST);
					}
					break;
				}
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
