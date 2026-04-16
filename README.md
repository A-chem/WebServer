# Webserv Test Environment

## Directory Structure

```
configs/
    main.conf           — single server, all features
    multi_server.conf   — two servers on different ports
    edge_cases.conf     — body limits, method restrictions

www/
    static/             — root for main server
        index.html      — main test page
        about.html      — second page
        style.css       — CSS (tests text/css MIME)
        sample.txt      — plain text (tests text/plain MIME)
        deleteme.txt    — file to test DELETE

    images/             — served with autoindex on
        readme.txt
        test.png
        photo.jpg
        thumbnails/     — subdirectory (tests autoindex dirs)

    errors/             — custom error pages
        404.html
        403.html
        500.html

    api/                — root for second server (port 9090)
        api.html

uploads/                — upload_store directory (POST writes here)

test.sh                 — automated test script
```

---

## Quick Start

### 1. Compile

```bash
g++ -Wall -Wextra -std=c++98 -I inc \
    src/main.cpp \
    src/config/Lexer.cpp \
    src/config/Parser.cpp \
    src/config/ConfigNode.cpp \
    src/config/ConfigValidator.cpp \
    src/config/ConfigLoader.cpp \
    src/utils/Utils.cpp \
    src/server/Client.cpp \
    src/server/Socket.cpp \
    src/server/Epoll.cpp \
    src/server/Server.cpp \
    src/server/Request.cpp \
    src/server/ErrorPages.cpp \
    src/server/Response.cpp \
    -o webserv
```

### 2. Run

```bash
./webserv configs/main.conf
```

### 3. Run automated tests

```bash
./test.sh
```

---

## Manual Test Commands

### GET — Static files
```bash
curl -v http://localhost:8080/
curl -v http://localhost:8080/about.html
curl -v http://localhost:8080/style.css
curl -v http://localhost:8080/sample.txt
```

### GET — Autoindex
```bash
curl -v http://localhost:8080/images/
# should return HTML directory listing
```

### GET — Directory without slash (redirect)
```bash
curl -v http://localhost:8080/images
# expect 301 -> /images/
```

### GET — Custom error pages
```bash
curl -v http://localhost:8080/nonexistent
# expect 404 with custom HTML
```

### GET — Redirects
```bash
curl -v http://localhost:8080/redirect
# expect 301 Location: http://localhost:8080/

curl -L http://localhost:8080/redirect
# follow redirect, should land on index
```

### POST — File upload
```bash
curl -v -X POST http://localhost:8080/upload --data "hello world"
# expect 201 Created
# check uploads/ directory for the file

curl -v -X POST http://localhost:8080/upload \
    -H "Content-Disposition: attachment; filename=\"myfile.txt\"" \
    --data "named upload content"
# check uploads/myfile.txt
```

### DELETE — File deletion
```bash
# verify file exists first
curl -v http://localhost:8080/files/deleteme.txt

# delete it
curl -v -X DELETE http://localhost:8080/files/deleteme.txt
# expect 204 No Content

# verify it's gone
curl -v http://localhost:8080/files/deleteme.txt
# expect 404 Not Found
```

### Method Not Allowed
```bash
curl -v -X DELETE http://localhost:8080/index.html
# expect 405 Method Not Allowed

curl -v -X POST http://localhost:8080/index.html --data "test"
# expect 405 Method Not Allowed
```

### Keep-Alive
```bash
curl -v --http1.1 http://localhost:8080/index.html
# look for Connection: keep-alive in response headers

curl -v -H "Connection: close" http://localhost:8080/index.html
# look for Connection: close in response headers
```

### Path Traversal (should be blocked)
```bash
curl -v "http://localhost:8080/../etc/passwd"
# expect 400 Bad Request

curl -v "http://localhost:8080/images/../../../etc/passwd"
# expect 400 Bad Request
```

### Body Size Limit
```bash
# start edge_cases.conf server (100 byte limit)
./webserv configs/edge_cases.conf

# send more than 100 bytes
curl -v -X POST http://localhost:8181/ --data "$(python3 -c "print('x'*200)")"
# expect 413 Content Too Large
```

### Multi-Server
```bash
# start multi server config
./webserv configs/multi_server.conf

# test both ports
curl -v http://localhost:8080/
curl -v http://localhost:9090/
```

### HTTP/1.0 vs HTTP/1.1
```bash
curl -v --http1.0 http://localhost:8080/
# expect Connection: close

curl -v --http1.1 http://localhost:8080/
# expect Connection: keep-alive
```

---

## What Each Config Tests

| Config | Port | Tests |
|--------|------|-------|
| main.conf | 8080 | All features — GET, POST, DELETE, autoindex, redirects, error pages |
| multi_server.conf | 8080 + 9090 | Two servers simultaneously, virtual hosting |
| edge_cases.conf | 8181 | Body size limits, strict method restrictions, redirect chains |

---

## Expected Behavior Matrix

| Request | Expected |
|---------|----------|
| GET existing file | 200 OK |
| GET nonexistent file | 404 custom page |
| GET directory without / | 301 redirect |
| GET directory with / + index | 200 index file |
| GET directory with / + autoindex | 200 listing |
| GET directory with / no index no autoindex | 403 |
| POST with upload_store | 201 Created |
| POST without upload_store | 204 No Content |
| DELETE existing file | 204 No Content |
| DELETE nonexistent file | 404 |
| Wrong method for location | 405 |
| Body exceeds limit | 413 |
| Path traversal | 400 |
| Redirect location | 301/302 |
