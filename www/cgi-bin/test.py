#!/usr/bin/env python3
import os
import sys

body = sys.stdin.read()

print('Content-Type: text/plain')
print()
print('method=' + os.environ.get('REQUEST_METHOD', ''))
print('query=' + os.environ.get('QUERY_STRING', ''))
print('content_length=' + os.environ.get('CONTENT_LENGTH', ''))
print('path_info=' + os.environ.get('PATH_INFO', ''))
print('body=' + body)
