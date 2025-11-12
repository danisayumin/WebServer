#!/usr/bin/python3

import sys

# Output HTTP headers with proper line endings
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.flush()

# This will cause an error - division by zero
result = 1 / 0