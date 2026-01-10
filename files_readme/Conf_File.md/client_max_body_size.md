## Config_file

`client_max_body_size`
    client_max_body_size 10K;
    - This directive sets the maximum allowed size of the HTTP request body.
    - It applies to requests that send a body, mainly:POST, PUT, PATCH Sometimes DELETE
    - Value meaning: 10K = 10 * 1024 bytes = 10240 bytes
        - The server will reject any request whose body is larger than 10240 bytes.
    
    1- Why does this exist?
        - To protect the server from:
            - Huge uploads
            - Memory exhaustion
            - DoS attacks
            - Slow clients sending infinite data
    
    2- When is this limit checked?
        - The server checks:
            - Before reading the body
            - Or while reading the body
        - Depending on:
            - Whether Content-Length is present
            - Or the body is being streamed

    3- HTTP/1.0 Behavior:
        - How HTTP/1.0 sends body:
            - In HTTP/1.0: 
                - There is NO chunked transfer encoding
                - The body size is determined ONLY by: Content-Length
    
    4- All Real Cases:
        - Case 1 — Valid request (Body smaller than limit)
            POST /upload HTTP/1.0
            Content-Length: 5000
            - 5000 bytes ≤ 10240 bytes -> yes
            - Server behavior:
                - Accept request
                - Read body
                - Process it normally
                - Return 200 OK or 201 Created
        - Case 2 — Body exactly equal to limit : Content-Length: 10240
            - 10240 bytes == 10240 bytes -> Yes
            - Server behavior:
                - Accept request
                - Read body
                - Process it normally

        - Case 3 — Body larger than limit: Content-Length: 20000
            - 20000 bytes > 10240 bytes -> No
            - Server behavior:
                - DO NOT read the body
                - Immediately respond: 413 Payload Too Large
        - Case 4 — No Content-Length in HTTP/1.0
            POST /upload HTTP/1.0
            (no Content-Length)
            - In HTTP/1.0, body size cannot be known
                - Server behavior:
                - Must reject request
                - Return: 411 Length Required
        - Case 5 — Invalid Content-Length: Content-Length: -50 or Content-Length: abc ... etc
            - Invalid header 
            - Server behavior: Return: 400 Bad Request
        - Case 6 — Client sends more than announced
            Content-Length: 5000
            But client sends 20000 bytes
            Server behavior:
                - Stop reading at 10240
                - Or stop at Content-Length
                - Close connection
                - Return: 413 Payload Too Large or 400 Bad Request
        - Case 7 — Server config is invalid 
            client_max_body_size -10K;
            client_max_body_size abc;
            - This is a configuration error
            - Server behavior:
                - Refuse to start
                - Print config error
                - Exit
    
    5- Order of Checks (Correct Logic in Webserv)
        - When receiving a request:
            - Parse headers
            - If method expects body:
            - If HTTP/1.0 and no Content-Length → 411
            - If Content-Length exists:
            - If invalid → 400
            - If > client_max_body_size → 413
            - Only if all OK:
                - Read body
                - Process reques

    6- How NGINX behaves
        - Nginx does exactly the same:
            - Rejects large body before reading
            - Returns 413
            - Enforces Content-Length in HTTP/1.0
            - Refuses invalid config at startup
    
    7- Can there be two client_max_body_size directives in the same config file? What happens?
        - Nginx allows multiple client_max_body_size in different contexts
            http {
                client_max_body_size 10K;

                server {
                    client_max_body_size 20K;
                }
            }
            - The most specific context wins:
                - server overrides http
                - location overrides server
            - Only one value per context is used.
            - If you put two directives in the same context, Nginx will use the last one: 
                server {
                    client_max_body_size 10K;
                    client_max_body_size 20K;  # this one overrides the 10K
                }
    
    8- how you should implement
        - Only one active value per context (server or location)
        - If multiple appear in the same server block, the last one should override previous ones
        - If you want strict checking, you can make duplicate directives a config error
        server {
            listen 127.0.0.1:8080;
            client_max_body_size 10K;
            client_max_body_size 15K;  # last one wins → 15K
        }
        - Webserv behavior:
            - Parse client_max_body_size for each line
            - Store value in server config
            - If another client_max_body_size is found, overwrite the old value
    
    9- Edge Cases
        - Two different sizes in nested contexts:
            server {
                client_max_body_size 10K;
                location /upload {
                    client_max_body_size 50K;
                }
            }
            - /upload → 50K
            -other paths → 10K
        - Two same-size directives:
            server {
                client_max_body_size 10K;
                client_max_body_size 10K;
            }
            - Acceptable
            - Last one wins → still 10K
        - Invalid second value:
            server {
                client_max_body_size 10K;
                client_max_body_size -5K;  # invalid → startup error
            }
            - Server must not start




    





