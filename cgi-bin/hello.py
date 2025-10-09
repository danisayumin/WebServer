#!/usr/bin/python

import os

# Corpo da resposta HTML
request_method = os.environ.get("REQUEST_METHOD", "(Not set)")
html_body = f"""
<!DOCTYPE html>
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
    <p>Your request method was: <strong>{request_method}</strong></p>
</body>
</html>
"""

# Cabeçalhos HTTP (usando sintaxe Python 3)
print("Content-Type: text/html")
print(f"Content-Length: {len(html_body)}")
print() # Linha em branco para separar cabeçalhos do corpo

# Corpo da resposta
print(html_body)