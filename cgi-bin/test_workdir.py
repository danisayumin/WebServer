#!/usr/bin/python3
import sys
import os

sys.stdout.write("Content-Type: text/plain\r\n")
sys.stdout.write("\r\n")

# Try to read a file from a relative path
try:
    with open("www/test.txt", "r") as f:
        content = f.read()
    sys.stdout.write("Successfully read file from relative path!\n")
    sys.stdout.write("Current working directory: {}\n".format(os.getcwd()))
    sys.stdout.write("File contents:\n")
    sys.stdout.write(content)
except IOError as e:
    sys.stdout.write("Failed to read file: {}\n".format(str(e)))
    sys.stdout.write("Current working directory: {}\n".format(os.getcwd()))

sys.stdout.flush()
