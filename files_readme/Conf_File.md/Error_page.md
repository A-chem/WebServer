## Config_file

`error_page directive`
    error_page 404 ./www/errors/404.html;
    - tells your server: “if a request fails with this HTTP error code, serve this file instead of the default error page.”
    - You can define multiple error codes: error_page 404 403 500 ./www/errors/error.html;
        - The path is relative to the server root or absolute. 
    1- What it does internally
        - Request handling starts: 
            - Client requests a resource: GET /index.html HTTP/1.0
            - Server tries to find /index.html.
        - Error occurs (file missing / forbidden / internal error):
            - Example: /notfound.html → 404
            - Server checks if there is an error_page configured for 404.
            - If yes → server serves the custom HTML file instead of the default error message.
            - If the custom HTML file itself is missing → server sends hardcoded HTTP response:
                HTTP/1.0 404 Not Found
                Content-Type: text/html
                <html><body><h1>404 Not Found</h1></body></html>
    2- HTTP/1.0 behavior
        - HTTP/1.0 is simpler than 1.1: no chunked encoding, no persistent connection by default.
        - Server must send full headers, including:
            HTTP/1.0 <status_code> <status_text>\r\n
            Content-Length: <length_of_body>\r\n
            Content-Type: text/html\r\n
            \r\n
            <body>
        - Example for 404 custom error page:
            HTTP/1.0 404 Not Found
            Content-Length: 245
            Content-Type: text/html
            [contents of ./www/errors/404.html]
        - Fallback behavior: if the error page file is missing:
            HTTP/1.0 404 Not Found
            Content-Length: 48
            Content-Type: text/html
            <html><body><h1>404 Not Found</h1></body></html>
    
    3- Edge cases / subtleties
        - Missing error page file:
            - Must send hardcoded HTML.
        - Permission denied on error page file:
            - Treat as missing → send hardcoded page.
        - Redirects or relative paths:
            - ./www/errors/404.html is relative to server root.
            - Absolute paths also allowed (/var/www/errors/404.html).
        - Multiple error codes to same page: error_page 404 403 ./www/errors/error.html;
            - Both 404 and 403 will serve error.html.
        - Interaction with index directive:
             - If the request is a directory: 
                - Server looks for index (e.g., index.html) inside directory.
                - If index is missing → 404 → triggers error_page.
            - Example: GET /folder/ HTTP/1.0
                - /www/folder/ exists, but /www/folder/index.html missing → 404 → serve error page.



