## Config_file

`listen Directive`
    1- listen <IP>:<PORT>:
        This tells your server: "Hey, open a socket (an endpoint for network communication) on this IP and port, and wait for incoming connections".
        <listen 127.0.0.1:8080>
            My server program will accept connections sent to this machine at IP 127.0.0.1 on port 8080.
        - Key points:
            - IP = the interface to bind to:
                - 127.0.0.1 → localhost only, meaning only this machine can connect.
                -0.0.0.0→ all interfaces, meaning any machine can connect.
            - PORT = the TCP port to listen on:
                - 8080 or 80 etc.
                - The port is how the OS knows which process should handle the incoming request.
                - If you omit IP, it defaults to 0.0.0.0.
                - If you omit port, usually defaults to 80 for HTTP.

    2- Each listen creates a separate socket:
        - A socket is like a phone line that waits for calls.
        - If you write: listen 127.0.0.1:8080 or listen 0.0.0.0:8081.
            - You are opening two phone lines:
                - One only for local machine (127.0.0.1) on port 8080.
                - One for all machines (0.0.0.0) on port 8081.
        -The server can now accept connections on both sockets independently.
    
    3- Multiple listen in the same server block → multi-port or multi-IP listening:
        - Multi-port: You can accept requests on different ports. Example: 8080 and 8081.
        - Multi-IP: You can accept requests on multiple IP addresses.
        - This is common when a server has multiple network interfaces or needs to support multiple ports.
    
    4- Why is this important?
        - Allows your server to serve multiple networks.
        - Allows your server to offer the same service on multiple ports.
        - You don’t need multiple server blocks if all the settings (root, index, etc.) are the same.

    5- Valid Cases:
        - Single IP & port: listen 127.0.0.1:8080.
            - Server listens only on localhost (127.0.0.1) at port 8080.
            - Clients: Only clients running on the same machine can connect.
            - Socket: One socket bound to (127.0.0.1, 8080).
            - HTTP/1.0: Each request is independent; after response, connection closes automatically.
        - All interfaces: listen 0.0.0.0:8080.
            - 0.0.0.0 = all interfaces (localhost + network IPs).
            - One socket listens on every IP address your server has at port 8080.
            - Clients: Any client that can reach your machine (localhost, LAN, public IP if allowed).
            - Important: Useful if server has multiple NICs and you want to serve all networks.
        - Multiple ports: listen 127.0.0.1:8080 & listen 127.0.0.1:8081.
            - One server block listens on two ports (8080 and 8081) for the same IP.
            - Sockets: Two independent sockets: (127.0.0.1, 8080) and (127.0.0.1, 8081).
            - Clients: Can connect to either port.
            - HTTP/1.0: Still stateless, no keep-alive; each request closes the connection.
            - Useful if you want to offer the same service on multiple ports.
        - Multiple servers on different ports:
            server { listen 127.0.0.1:8080; }
            server { listen 127.0.0.1:8081; }
            - Two server blocks on same host but different ports.
            - Perfectly fine; each server binds to its own socket.
            - Clients: Connect to port-specific server.
        - Multiple servers, same port, different IPs:
            server { listen 127.0.0.1:8080; }
            server { listen 192.168.1.2:8080; }
            - Two servers share the same port (8080), but on different IP addresses.
            - Sockets: No conflict → OS differentiates by IP:port pair.
            - Clients: Requests routed by which IP they connect to.
        -IPv6 support: listen [::1]:8080.
            - Valid IPv6 localhost.
            - Works like 127.0.0.1 but in IPv6.
            - Syntax requires brackets [ ] around IPv6 address.
        - Wildcard IP + port: listen 8080.
            - IP omitted → defaults to 0.0.0.0.
            - Server listens on all interfaces at port 8080.
    
    6- Invalid / Error Cases:
        - Port out of range: listen 127.0.0.1:99999;  # invalid port (>65535)
            - Server fails to start → bind() error.
        - Invalid IP: listen 300.300.0.1:8080.
            - Server startup error → IP format invalid.
        - Two servers same IP & port, no host distinction:
            server {
                listen 127.0.0.1:8080;
            }
            server {
                listen 127.0.0.1:8080;
            }
            - Technically valid at socket level → only one bind() allowed per IP:PORT.
            - Usually server frameworks detect duplication and fail or merge.
            - HTTP/1.0 → uses first matching server if no Host header.
    7- Edge Cases in HTTP/1.0:
        - Host header might not exist:
            - HTTP/1.0 clients often omit Host.
            - Server chooses first matching IP:port server.
        - Persistent connection is irrelevant:
            - HTTP/1.0 closes connection after each response automatically.
            - No keep-alive needed.
        - Multiple clients:
            - Multiple clients can connect simultaneously on different sockets (IP:port combinations).
    