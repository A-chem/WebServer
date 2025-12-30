void parseServer(const std::vector<std::string> &tokens, size_t &i, ServerConfig &server) {
    while (i < tokens.size() && tokens[i] != "}") {
        std::string token = tokens[i];

        if (token == "listen") {
            i++;
            std::string value = tokens[i++];
            if (tokens[i] != ";") throw std::runtime_error("Missing ; after listen");
            server.listen.push_back(std::make_pair("", std::atoi(value.c_str())));
        }
        else if (token == "root") server.root = parseSingleValue(tokens, i);
        else if (token == "index") server.index = parseSingleValue(tokens, i);
        else if (token == "client_max_body_size") server.client_max_body_size = parseSize(tokens, i);
        else if (token == "error_page") {
            i++;
            int code = std::atoi(tokens[i].c_str());
            i++;
            std::string path = tokens[i++];
            if (tokens[i++] != ";") throw std::runtime_error("Missing ;");
            server.error_pages[code] = path;
        }
        else if (token == "location") {
            i++;
            LocationConfig loc;
            loc.path = tokens[i++];
            if (tokens[i++] != "{") throw std::runtime_error("Expected { after location");
            parseLocation(tokens, i, loc);
            server.locations.push_back(loc);
        }
        else {
            throw std::runtime_error("Unknown directive in server block: " + token);
        }
    }
    if (tokens[i++] != "}") throw std::runtime_error("Missing closing } for server");
}
