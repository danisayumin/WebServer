#!/usr/bin/python3

import sys

# CGI scripts MUST output headers followed by a blank line, then body
# Headers must end with \r\n, and blank line must be \r\n\r\n

body = "Hello from simple Python CGI!"

# Output HTTP headers with proper line endings
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("Content-Length: {}\r\n".format(len(body)))
sys.stdout.write("\r\n")

# Output body
sys.stdout.write(body)
sys.stdout.flush()
