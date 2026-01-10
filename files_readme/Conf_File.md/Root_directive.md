## Config_file

`root Directive`
    1- Meaning:
        root ./www/main;
        - root defines the base directory in the filesystem where the server will search for files to serve.
        - It is NOT a URL
        - It is a filesystem path.
        - The client never accesses your real OS filesystem directly.

    2- How a request becomes a real file path: 
        - When a client sends: GET /images/logo.png HTTP/1.0.
        - Your server does:
            Final path = root + request_path
            Final path = ./www/main + /images/logo.png
            Final path = ./www/main/images/logo.png
        - root is the filesystem base directory for URL paths

    3- Important rule:
        - The client never knows your real filesystem.
        - Client sees: /images/logo.png
        - Server maps it to: ./www/main/images/logo.png

    4- How it works internally:
        - When a request comes: GET /index.html HTTP/1.0
        - The server takes the root path from config: root = "./www/main"
        - Combines it directly with the request path: ./www/main + /index.html → ./www/main/index.html
        - Opens the file → sends content if exists → returns 200 OK.
    
    5- Edge cases:
        - File exists: GET /images/logo.png → ./www/main/images/logo.png exists → 200 OK
        - File does not exist: GET /notfound.html → ./www/main/notfound.html does not exist → 404 Not Found
        - Directory requested: GET / → ./www/main/ → try index.html (if exists)
            - If index.html exists → 200 OK
            - If not → 404 Not Found (or 403 if you want,  OK with 404)
            - Path traversal attempts: GET /../secret.txt → ./www/main/../secret.txt → server may serve it (if y want)
    
    6- Parsing the root from config:
        - Inside server block:
            server {
                root ./www/main;
            }
        - Inside location block:
            location /images {
                root ./www/images;
            }
        - Logic: if location has root → use it else → use server root
            - Duplicate root in same block → config error
            - Missing semicolon or path → config error




