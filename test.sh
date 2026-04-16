#!/bin/bash

# Webserv Test Script
# Tests every feature of the server
# Usage: ./test.sh [host] [port]
# Default: localhost 8080

HOST=${1:-localhost}
PORT=${2:-8080}
BASE="http://$HOST:$PORT"
PASS=0
FAIL=0

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

pass() { echo -e "${GREEN}[PASS]${NC} $1"; PASS=$((PASS+1)); }
fail() { echo -e "${RED}[FAIL]${NC} $1"; FAIL=$((FAIL+1)); }
header() { echo -e "\n${YELLOW}=== $1 ===${NC}"; }

check_status() {
    local desc=$1
    local expected=$2
    local url=$3
    local method=${4:-GET}
    local data=${5:-}

    if [ -n "$data" ]; then
        actual=$(curl -s -o /dev/null -w "%{http_code}" -X $method "$url" --data "$data")
    else
        actual=$(curl -s -o /dev/null -w "%{http_code}" -X $method "$url")
    fi

    if [ "$actual" = "$expected" ]; then
        pass "$desc (got $actual)"
    else
        fail "$desc (expected $expected, got $actual)"
    fi
}

check_body() {
    local desc=$1
    local pattern=$2
    local url=$3

    body=$(curl -s "$url")
    if echo "$body" | grep -q "$pattern"; then
        pass "$desc"
    else
        fail "$desc (pattern '$pattern' not found)"
    fi
}

check_header() {
    local desc=$1
    local pattern=$2
    local url=$3

    headers=$(curl -sI "$url")
    if echo "$headers" | grep -qi "$pattern"; then
        pass "$desc"
    else
        fail "$desc (header '$pattern' not found)"
    fi
}

echo "======================================"
echo " Webserv Test Suite"
echo " Target: $BASE"
echo "======================================"

# ==========================================
header "1. Static File Serving (GET)"
# ==========================================

check_status "GET /index.html returns 200" "200" "$BASE/index.html"
check_status "GET /about.html returns 200" "200" "$BASE/about.html"
check_status "GET /style.css returns 200" "200" "$BASE/style.css"
check_status "GET /sample.txt returns 200" "200" "$BASE/sample.txt"

# ==========================================
header "2. MIME Types"
# ==========================================

check_header "HTML served as text/html" "text/html" "$BASE/index.html"
check_header "CSS served as text/css" "text/css" "$BASE/style.css"
check_header "TXT served as text/plain" "text/plain" "$BASE/sample.txt"

# ==========================================
header "3. Error Pages"
# ==========================================

check_status "GET nonexistent returns 404" "404" "$BASE/nonexistent.html"
check_body "Custom 404 page served" "404" "$BASE/nonexistent.html"

# ==========================================
header "4. Autoindex"
# ==========================================

check_status "GET /images/ returns 200" "200" "$BASE/images/"
check_body "Autoindex contains file listing" "readme.txt" "$BASE/images/"
check_body "Autoindex contains subdirectory" "thumbnails" "$BASE/images/"

# ==========================================
header "5. Directory Redirect"
# ==========================================

check_status "GET /images without slash returns 301" "301" "$BASE/images"

# ==========================================
header "6. Redirects"
# ==========================================

check_status "GET /redirect returns 301" "301" "$BASE/redirect"
check_status "GET /old-page returns 301" "301" "$BASE/old-page"

# Check Location header
redirect_loc=$(curl -sI "$BASE/redirect" | grep -i "location" | tr -d '\r')
if echo "$redirect_loc" | grep -q "localhost:8080"; then
    pass "Redirect Location header correct"
else
    fail "Redirect Location header incorrect: $redirect_loc"
fi

# ==========================================
header "7. POST — File Upload"
# ==========================================

check_status "POST /upload returns 201" "201" "$BASE/upload" "POST" "test upload data"

# Upload with binary data
actual=$(curl -s -o /dev/null -w "%{http_code}" -X POST "$BASE/upload" \
    --data-binary "binary upload test content")
if [ "$actual" = "201" ]; then
    pass "POST binary upload returns 201"
else
    fail "POST binary upload (expected 201, got $actual)"
fi

# ==========================================
header "8. DELETE"
# ==========================================

# First verify file exists
check_status "GET /files/deleteme.txt returns 200 before delete" "200" "$BASE/files/deleteme.txt"

# Delete it
check_status "DELETE /files/deleteme.txt returns 204" "204" "$BASE/files/deleteme.txt" "DELETE"

# Verify it's gone
check_status "GET /files/deleteme.txt returns 404 after delete" "404" "$BASE/files/deleteme.txt"

# Delete nonexistent
check_status "DELETE nonexistent returns 404" "404" "$BASE/files/nonexistent.txt" "DELETE"

# ==========================================
header "9. Method Not Allowed"
# ==========================================

check_status "DELETE on GET-only location returns 405" "405" "$BASE/index.html" "DELETE"
check_status "POST on GET-only location returns 405" "405" "$BASE/index.html" "POST" "data"

# ==========================================
header "10. Keep-Alive"
# ==========================================

# HTTP/1.1 keep-alive
ka_header=$(curl -s -I --http1.1 "$BASE/index.html" | grep -i "connection" | tr -d '\r')
if echo "$ka_header" | grep -qi "keep-alive"; then
    pass "HTTP/1.1 keep-alive header present"
else
    fail "HTTP/1.1 keep-alive header missing: $ka_header"
fi

# ==========================================
header "11. Path Traversal Protection"
# ==========================================

check_status "Path traversal blocked" "400" "$BASE/../etc/passwd"
check_status "Encoded traversal attempt" "400" "$BASE/%2e%2e/etc/passwd"

# ==========================================
header "12. Body Size Limit"
# ==========================================

# Generate data larger than 100 bytes for edge_cases server
# (requires edge_cases.conf server to be running on 8181)
# Skipping if not running
edge_status=$(curl -s -o /dev/null -w "%{http_code}" "http://localhost:8181/" 2>/dev/null)
if [ "$edge_status" != "000" ]; then
    large_data=$(python3 -c "print('x' * 200)")
    check_status "POST over body limit returns 413" "413" \
        "http://localhost:8181/" "POST" "$large_data"
else
    echo -e "${YELLOW}[SKIP]${NC} Edge cases server not running (start with configs/edge_cases.conf)"
fi

# ==========================================
echo ""
echo "======================================"
echo -e " Results: ${GREEN}$PASS passed${NC} / ${RED}$FAIL failed${NC}"
echo "======================================"

if [ $FAIL -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
else
    exit 1
fi
