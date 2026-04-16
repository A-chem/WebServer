#include "Utils.hpp"
#include <algorithm> // 3la wdit transform

bool	isNumeric(const std::string &s) {
	if (s.empty()) return false;
	for (size_t i = 0; i < s.size(); i++) {
		if (!std::isdigit(s[i])) return false;
	}
	return true;
}

void	parseListenValue(const std::string& vl) {
	std::string	ip;
	std::string	port_str;
	size_t	pos = vl.find(':');

	if (pos == std::string::npos) {
		if (isNumeric(vl)) { ip = "0.0.0.0"; port_str = vl;}
		else { ip = vl; port_str = "80";}
	}
	else {
		ip = vl.substr(0, pos);
		port_str = vl.substr(pos + 1);
		if (ip.empty() || port_str.empty()) throw std::runtime_error("Invalid listen: invalid format '" + vl + "'");
	}
	// hna l9it bli ida kan Localhost or anything else ghadi tn9z had check o tkhli getaddrinfo dir resolve rassha hiya mahiya
	std::transform(ip.begin(), ip.end(), ip.begin(), ::tolower);
	if (ip == "localhost") ip = "127.0.0.1";
	struct addrinfo hints;
	struct addrinfo *res;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	int ret = getaddrinfo(ip.c_str(), NULL, &hints, &res);
	if (ret != 0) throw std::runtime_error("Invalid host: " + vl + "--->" + gai_strerror(ret) + "<---");
	freeaddrinfo(res);

	// port handlnig now 
	if (!isNumeric(port_str)) throw std::runtime_error("listen: port must be numeric '" + port_str + "'");
	int port = std::atoi(port_str.c_str());
	if (port < 1 || port > 65535) throw std::runtime_error("Invalid listen: port out of range'" + port_str + "'");
}

void	parse_http_code(const std::string& nb) {
	if (isNumeric(nb)) {
		int code = std::atoi(nb.c_str());
		if (code < 100 || code > 599 || nb.size() > 3) throw std::runtime_error("Error_page code (out of range 100-599): '" + nb + "'");
	} else throw std::runtime_error("Error_page code must be numeric'" + nb + "'");
}

void	parseCbmz(const std::string& vl) {
	std::string nb;
	char last = vl[vl.size() - 1];
	if (last == 'k' || last == 'K' || last == 'm' || last == 'M' || last == 'g' || last == 'G') {
		nb = vl.substr(0, vl.size() - 1);
		if (!isNumeric(nb)) throw std::runtime_error("client_max_body_size: invalid value'" + nb + "'");
	} else if (!isNumeric(vl)) throw std::runtime_error("client_max_body_size: invalid value'" + vl + "'");
}

// functions used on ConfigLoader (extraction had lmra machi validation)

std::pair<std::string, int> parseListen(const std::string& val) {
	std::string	ip;
	std::string	port_str;
	size_t	pos = val.find(':');

	if (pos == std::string::npos) {
		if (isNumeric(val)) { ip = "0.0.0.0"; port_str = val; }
		else { ip = val; port_str = "80"; }
	}
	else {
		ip = val.substr(0, pos);
		port_str = val.substr(pos + 1);
	}
	int port = std::atoi(port_str.c_str());
	return std::make_pair(ip, port);
}

size_t	parseSize(const std::string& val) {
	if (val.empty()) return 0;
	size_t	multiplier = 1;
	std::string num_str = val;
	char last = val[val.size() - 1];

	if (last == 'k' || last == 'K') { multiplier = 1024; num_str = val.substr(0, val.size() - 1);}
	else if (last == 'm' || last == 'M' ) { multiplier = 1024 * 1024; num_str = val.substr(0, val.size() - 1);}
	else if (last == 'g' || last == 'G' ) {multiplier = 1024 * 1024 * 1024; num_str = val.substr(0, val.size() -1);}

	return static_cast<size_t>(std::atoi(num_str.c_str())) * multiplier;
}
