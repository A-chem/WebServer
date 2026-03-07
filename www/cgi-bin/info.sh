#!/bin/bash

# 1. HTTP Header (Darori)
echo -e "Content-Type: text/html\r\n\r\n"

# 2. HTML Content
echo "<html>"
echo "<head><title>CGI Bash Info</title></head>"
echo "<body style='font-family: monospace; background: #222; color: #0f0; padding: 20px;'>"
echo "<h1>🖥️ Server CGI Environment (Bash)</h1>"
echo "<hr>"

echo "<h3>Environment Variables:</h3>"
echo "<ul>"
echo "  <li><b>Method:</b> $REQUEST_METHOD</li>"
echo "  <li><b>Path Info:</b> $PATH_INFO</li>"
echo "  <li><b>Remote Addr:</b> $REMOTE_ADDR</li>"
echo "  <li><b>User Agent:</b> $HTTP_USER_AGENT</li>"
echo "  <li><b>Query String:</b> $QUERY_STRING</li>"
echo "</ul>"

echo "<h3>System Info:</h3>"
echo "<pre>"
echo "Date: $(date)"
echo "Uptime: $(uptime)"
echo "Current Directory: $(pwd)"
echo "</pre>"

echo "<hr>"
echo "<a href='/' style='color: white;'>⬅️ Back to Home</a>"
echo "</body>"
echo "</html>"