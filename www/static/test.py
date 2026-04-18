#!/usr/bin/env python3
import os
import sys

method = os.environ.get('REQUEST_METHOD', '')
query = os.environ.get('QUERY_STRING', '')
body = sys.stdin.read()

print('Content-Type: text/plain')
print()
print('method=' + method)
print('query=' + query)
print('body=' + body)
