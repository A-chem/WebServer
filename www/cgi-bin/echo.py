#!/usr/bin/env python3
import os
import sys

body = sys.stdin.read()

print("Status: 200 OK")
print("Content-Type: text/html")
print()
print("<!doctype html>")
print("<html><head><title>Python CGI Test</title></head><body>")
print("<h1>Python CGI OK</h1>")
print("<p>Method: {}</p>".format(os.environ.get("REQUEST_METHOD", "")))
print("<p>Query: {}</p>".format(os.environ.get("QUERY_STRING", "")))
print("<p>Script: {}</p>".format(os.environ.get("SCRIPT_NAME", "")))
print("<pre>{}</pre>".format(body))
print("</body></html>")
