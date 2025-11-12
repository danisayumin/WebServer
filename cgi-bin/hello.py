#!/usr/bin/python3

import sys
import os

# Get REQUEST_METHOD from environment
request_method = os.environ.get("REQUEST_METHOD", "(Not set)")

# Create HTML body
html_body = """<!DOCTYPE html>
<html>
<head>
    <title>CGI Hello</title>
    <style>
        body {{ font-family: sans-serif; background-color: #222; color: #0f0; text-align: center; margin-top: 50px; }}
        h1 {{ border: 1px solid #0f0; padding: 10px; }}
        p {{ color: #aaa; }}
    </style>
</head>
<body>
    <h1>Hello from a Python CGI Script!</h1>
    <hr>
    <p>This page was generated dynamically by executing a Python script.</p>
    <p>Your request method was: <strong>{}</strong></p>
</body>
</html>""".format(request_method)

# Output HTTP headers with proper line endings
sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("Content-Length: {}\r\n".format(len(html_body)))
sys.stdout.write("\r\n")

# Output body
sys.stdout.write(html_body)
sys.stdout.flush()