#!/usr/bin/python3

import sys
import os

# Read request body from stdin
content_length = int(os.environ.get("CONTENT_LENGTH", 0))
request_body = sys.stdin.read(content_length) if content_length > 0 else ""

# Create HTML body
html_body = """<!DOCTYPE html>
<html>
<head>
    <title>CGI POST Test</title>
    <style>
        body {{ font-family: monospace; background-color: #222; color: #0f0; }}
        h1, h2 {{ color: #0f0; }}
        pre {{ background-color: #111; padding: 10px; border: 1px solid #0f0; }}
    </style>
</head>
<body>
    <h1>CGI POST Request Received</h1>
    <h2>Environment Variables:</h2>
    <pre>
REQUEST_METHOD: {request_method}
CONTENT_LENGTH: {content_length}
CONTENT_TYPE: {content_type}
SCRIPT_NAME: {script_name}
QUERY_STRING: {query_string}
    </pre>
    <h2>Request Body (from stdin):</h2>
    <pre>
{request_body}
    </pre>
</body>
</html>"""

# Format with environment variables
html_body = html_body.format(
    request_method=os.environ.get("REQUEST_METHOD", ""),
    content_length=content_length,
    content_type=os.environ.get("CONTENT_TYPE", ""),
    script_name=os.environ.get("SCRIPT_NAME", ""),
    query_string=os.environ.get("QUERY_STRING", ""),
    request_body=request_body
)

# Output HTTP headers with proper line endings
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("Content-Length: {}\r\n".format(len(html_body)))
sys.stdout.write("\r\n")

# Output body
sys.stdout.write(html_body)
sys.stdout.flush()

