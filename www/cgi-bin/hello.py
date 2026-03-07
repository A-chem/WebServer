#!/usr/bin/python3
import os

print("Content-Type: text/html\r\n\r\n")
print("<html><head><title>CGI Result</title></head><body>")
print("<h1>Hello from Python CGI!</h1>")
print("<p><b>Request Method:</b> " + os.environ.get('REQUEST_METHOD', 'N/A') + "</p>")
print("<p><b>Server Protocol:</b> " + os.environ.get('SERVER_PROTOCOL', 'N/A') + "</p>")
print("<p><b>Remote Address:</b> " + os.environ.get('REMOTE_ADDR', 'N/A') + "</p>")
print("<br><a href='/'>Go Back</a>")
print("</body></html>")