##Config_File

1- listen Directive :

    server {
        listen 127.0.0.1:8080;
        listen 0.0.0.0:8081;
    }

    What it does:
        1- listen <IP>:<PORT> :
            This tells your server:
                “Hey, open a socket (an endpoint for network communication) on this IP and port, and wait for incoming connections.”

            Key points:
            -  IP = the interface to bind to.
                - 127.0.0.1 → localhost only, meaning only this machine can connect.
                - 0.0.0.0 → all interfaces, meaning any machine can connect.
            - PORT = the TCP port to listen on.
                - 8080 or 80 etc.
                - The port is how the OS knows which process should handle the incoming request.
            - If you omit IP, it defaults to 0.0.0.0.
            - If you omit port, usually defaults to 80 for HTTP.
        
        2- Each listen creates a separate socket:
            - A socket is like a phone line that waits for calls.
            - If you write: listen 127.0.0.1:8080 or listen 0.0.0.0:8081;
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
            - Single IP & port: listen 127.0.0.1:8080;
                - Server listens only on localhost, port 8080.
                - Only clients on the same machine can connect.
            - All interfaces: listen 0.0.0.0:8080;
                - Server listens on all network interfaces (localhost + network IPs) on port 8080.
            - Multiple ports: listen 127.0.0.1:8080 or listen 127.0.0.1:8081.
                - Single server block listening on two ports.
                - HTTP/1.0: each request is independent → no keep-alive handling.
            - Multiple servers on different ports:
                server {
                    listen 127.0.0.1:8080;
                }
                server {
                    listen 127.0.0.1:8081;
                }
                - Perfectly fine. Clients connect based on port.
            - Multiple servers, same port, different IPs:
                server {
                    listen 127.0.0.1:8080;
                }
                server {
                    listen 192.168.1.2:8080;
                }
                - Valid. Each server handles traffic on its specific IP.
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
            - Host header might not exist
                HTTP/1.0 clients often omit the Host header. Server chooses the first server that matches the IP:port.
            - Persistent connection is irrelevant
                HTTP/1.0 closes the connection after each response. No need for keep-alive logic.
            - Multiple clients
                Multiple clients can connect simultaneously to different IPs or ports.
            - IPv6 support: listen [::1]:8080.
                - Valid IPv6 listen. Treats ::1 as localhost in IPv6.
            - Wildcard IP + port: listen 8080;  # defaults to 0.0.0.0:8080.
                - Server listens on all interfaces on port 8080.
        8- Practical Rules for HTTP/1.0 Server:
            - Each socket (IP:port) → independent request handling.
            - If multiple servers share a port:
                - If Host header exists → route based on Host.
                - If Host header missing → fallback to first declared server.
            - No need for keep-alive/persistent connection handling.
            - Errors in listen directive → prevent server startup.

