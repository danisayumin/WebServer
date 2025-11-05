#!/usr/bin/env python

import sys
import os

def main():
    # Get the request body from stdin
    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
    request_body = sys.stdin.read(content_length)

    # Start HTTP response
    print("Content-Type: text/html\r\n\r\n")

    # HTML Response Body
    print("<html>")
    print("<head><title>CGI POST Test</title></head>")
    print("<body>")
    print("<h1>CGI POST Request Received</h1>")
    
    print("<h2>Environment Variables:</h2>")
    print("<ul>")
    for key, value in os.environ.items():
        print("<li><b>%s:</b> %s</li>" % (key, value))
    print("</ul>")

    print("<h2>Request Body (stdin):</h2>")
    print("<pre>")
    print(request_body)
    print("</pre>")

    print("</body>")
    print("</html>")

if __name__ == "__main__":
    main()

