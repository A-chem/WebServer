## Config_file

`index Directive`
    index index.html;
    - It tells the server: 
        "If a client requests a directory (like /), try to serve this file inside the directory first."
    - index.html is considered the default file for that directory.
    
    1- When is index used?
        - ONLY when: Resolved path is a DIRECTORY.
            - Example: GET / HTTP/1.0, GET /upload HTTP/1.0, GET /images/ HTTP/1.0
        - NOT used when: GET /file.html HTTP/1.0, GET /test.txt HTTP/1.0
    
    2- Full Request Resolution Pipeline (REAL SERVER LOGIC):
        - Given: server {
                    root ./www;
                    index index.html index.htm;
                    autoindex off;
                }
        - Request: GET /images HTTP/1.0
        - Step 1 — Build filesystem path
            fs_path = root + uri
            fs_path = ./www + /images
            fs_path = ./www/images
        - Step 2 — Does path exist?
            - No → 404 Not Found
            - Yes → continue
        - Step 3 — Is it a file or directory?
            - File → serve it → 200 OK
            - Directory → go to index logic
        - Step 4 — Index logic
            - Do we have an index list?
                - From location if exists
                - Else from server
            - If: index index.html index.htm;
                - Try in order: ./www/blog/index.html && ./www/blog/index.htm
            - If one exists: 200 OK Serve it
            - If NONE exist: Check: autoindex ON or OFF?
        - Step 5 — Autoindex logic
            - autoindex ON → 200 OK + directory listing
            - autoindex OFF → 403 Forbidden
    
    3- HTTP/1.0 Role:
        - In HTTP/1.0: GET / HTTP/1.0
            - No Host header
            -  No magic
            - Server MUST resolve / to something
            - / = directory → apply index algorithm
            - HTTP does NOT define index.
            - The SERVER does.
    
    4- Server index vs Location index:
        server {
            index index.html;

            location /admin {
                index dashboard.html;
            }
        }
        - if location has index → use it
        - else → use server index
        - NO fallback between them
        - If location defines index and it does not exist:
            - You DO NOT try server index
            - You go to autoindex / 403   
    
    5- If there is NO index directive AT ALL:
        server {
            root ./www;
            autoindex off;
        }
        - Request: GET / HTTP/1.0
            - Path = ./www/ → directory
            - No index list
            - autoindex OFF → 403 Forbidden
            - If autoindex ON → list directory.





