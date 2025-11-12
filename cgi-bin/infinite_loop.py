#!/usr/bin/python3

import sys
import time

# Output HTTP headers with proper line endings
sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")
sys.stdout.flush()

# Infinite loop - the server should handle timeout if needed
counter = 0
while True:
    time.sleep(1)
    sys.stdout.write("Looping... {}\n".format(counter))
    sys.stdout.flush()
    counter += 1
