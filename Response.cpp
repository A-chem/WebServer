#include "Server.hpp"
#include <sstream>
#include <dirent.h>

// --- CLIENT FILE HANDLING ---
void Client::openFile(const std::string &path) {
    file_stream.open(path.c_str(), std::ios::binary);
}

std::streamsize Client::readFile(char *buf, std::size_t size) {
    if (!file_stream.is_open()) return -1;
    file_stream.read(buf, size);
    return file_stream.gcount();
}

void Client::setFileSize(size_t fs) { file_size = fs; }
void Client::setHeaderSent(bool hs) { header_sent = hs; }
bool Client::headerSent() { return header_sent; }
void Client::setBytesSent(size_t bs) { bytes_sent += bs; }
size_t Client::getBytesSent() const { return bytes_sent; }
size_t Client::getFileSize() const { return file_size; }

// --- MIME TYPES ---
std::string getContentType(const std::string& path) {
    if (path.find(".html") != std::string::npos) return "text/html";
    if (path.find(".css") != std::string::npos) return "text/css";
    if (path.find(".js") != std::string::npos) return "application/javascript";
    if (path.find(".png") != std::string::npos) return "image/png";
    if (path.find(".jpg") != std::string::npos || path.find(".jpeg") != std::string::npos) return "image/jpeg";
    if (path.find(".mp4") != std::string::npos) return "video/mp4";
    return "text/plain";
}
std::string simpleAutoindex(const std::string& path) {
    DIR* dir = opendir(path.c_str());
    if (!dir) return "<html><body><h1>403 Forbidden</h1></body></html>";

    std::ostringstream html;
    html << "<html><head><title>Index of " "</title></head><body>";
    html << "<h1>Index of " "</h1><hr><pre>";

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        
        if (name == ".") continue;
        std::string full_path = path + "/" + name;
        struct stat st;
        std::string link = name;
        if (stat(full_path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
            link += "/";
        }
        html << "<a href=\"" << link << "\">" << link << "</a>\n";
    }

    html << "</pre><hr></body></html>";
    closedir(dir);
    return html.str();
}

// --- ROUTING & RESPONSE BUILDING ---
void Server::buildResponse(Client &c) {
    ServerConfig* matched_server = &_configs[0]; 
  

    LocationConfig* matched_loc = NULL;
    size_t longest_match = 0;
    
    // 1. Find the best matching Location
    for (size_t i = 0; i < matched_server->locations.size(); i++) {
        std::string loc_path = matched_server->locations[i].path;
        if (c.getPath().find(loc_path) == 0) { 
            if (loc_path.length() > longest_match) {
                longest_match = loc_path.length();
                matched_loc = &matched_server->locations[i];
            }
        }
    }

    std::string physical_path = "";
    if (matched_loc) {
        std::string root = matched_loc->root;
        std::string uri = c.getPath();
        std::string loc_path = matched_loc->path;

        // Remove trailing slash from root to standardize
        if (!root.empty() && root[root.size() - 1] == '/') {
            root.erase(root.size() - 1);
        }

        // Strip the location part from the URI (This is what you correctly pointed out!)
        // e.g., uri = "/images/pic.png", loc_path = "/images" -> remain = "/pic.png"
        std::string remain = uri.substr(loc_path.length());

        // Ensure safe joining with a '/'
        if (remain.empty() || remain[0] != '/') {
            remain = "/" + remain;
        }

        physical_path = root + remain;

        // If the original URI ended with '/', it's a directory request, append index
        if (!uri.empty() && uri[uri.length() - 1] == '/') {
            if (physical_path[physical_path.length() - 1] != '/') {
                physical_path += "/";
            }
            physical_path += matched_loc->index; 
        }
    }

    std::cout << "[ROUTING] Requested URL: " << c.getPath() << " -> Mapped to file: " << physical_path << std::endl;

    // 2. Check if file exists and get size
    struct stat st;
    std::ostringstream oss;

    if (stat(physical_path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
        c.openFile(physical_path);
        c.setFileSize(st.st_size);

        oss << "HTTP/1.1 200 OK\r\n";
        oss << "Server: Webserv/1.0\r\n";
        oss << "Content-Length: " << st.st_size << "\r\n";
        oss << "Content-Type: " << getContentType(physical_path) << "\r\n";
        oss << "Connection: close\r\n"; 
        oss << "\r\n";
        
        std::cout << "[RESPONSE] 200 OK" << std::endl;
    } 
    else {
        if (S_ISDIR(st.st_mode))
        {
            if(matched_server->locations[0].autoindex == 1)
            {
                std::string autoindex_body = simpleAutoindex(physical_path);
                oss << "HTTP/1.1 200 OK\r\n";
                oss << "Content-Type: text/html\r\n";
                oss << "Content-Length: " << autoindex_body.size() << "\r\n";
                oss << "Connection: close\r\n\r\n";
                oss << autoindex_body;
            }
        }
        else
        {
            std::string body404 = "<html><body><h1>404 Not Found</h1></body></html>";
            c.setFileSize(body404.size());
            
            oss << "HTTP/1.1 404 Not Found\r\n";
            oss << "Server: Webserv/1.0\r\n";
            oss << "Content-Length: " << body404.size() << "\r\n";
            oss << "Content-Type: text/html\r\n";
            oss << "Connection: close\r\n"; 
            oss << "\r\n";
            oss << body404;
    
            std::cout << "[RESPONSE] 404 Not Found" << std::endl;
        }

    }

    c.sendBuf() = oss.str();
}

// --- SENDING LOGIC ---
void Server::handleResponse(int fd) {
    Client &c = *clients[fd];
    char chunk[8192];

    // 1. Send Headers (and the 404 body if it was attached to headers)
    if (!c.headerSent()) {
        ssize_t n = send(fd, c.sendBuf().c_str(), c.sendBuf().size(), 0);
        if (n > 0) {
            c.sendBuf().erase(0, n);
            if (c.sendBuf().empty()) c.setHeaderSent(true);
        } else if (n == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return;
        } else {
            c.setState(CLOSED);
        }
        return; 
    }
    
    // 2. Send File Body (if a file was opened)
    if (c.file_stream.is_open() && c.getBytesSent() < c.getFileSize()) {
        c.file_stream.clear(); 
        c.file_stream.seekg(c.getBytesSent()); 

        std::streamsize actual_read = c.readFile(chunk, sizeof(chunk));
        if (actual_read <= 0) {
            c.setState(CLOSED);
            return;
        }

        ssize_t n = send(fd, chunk, actual_read, 0);
        if (n == -1) {
            if (errno == EPIPE || errno == ECONNRESET) {
                std::cout << "Client closed connection (Broken Pipe)" << std::endl;
                c.setState(CLOSED); 
                return;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return; 
            }
            c.setState(CLOSED);
            return;
        }
         
        if (n > 0) {
            c.setBytesSent(n);
        }
    }

    // 3. Finish
    if (c.getBytesSent() >= c.getFileSize()) {
        std::cout << "Done sending file to fd " << fd << std::endl;
        c.setState(CLOSED);
    }
}
