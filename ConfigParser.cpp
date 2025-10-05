#include "ConfigParser.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cstdlib>

// Helper function to trim whitespace from both ends of a string
static std::string trim(const std::string& s) {
    const std::string whitespace = " 	";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) {
        return ""; // String contains only whitespace
    }
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

ConfigParser::ConfigParser(const std::string& filePath) : _filePath(filePath), _port(0) {
    _defaultErrorPage = ""; // Initialize with a default
    try {
        parse();
    } catch (const std::exception& e) {
        std::cerr << "Error parsing config file: " << e.what() << std::endl;
        exit(EXIT_FAILURE); // Exit on critical parsing error
    }
}

ConfigParser::~ConfigParser() {}

void ConfigParser::parse() {
    std::ifstream configFile(_filePath.c_str());
    if (!configFile.is_open()) {
        throw std::runtime_error("Could not open file: " + _filePath);
    }

    std::string line;
    bool inServerBlock = false;

    while (std::getline(configFile, line)) {
        std::string trimmedLine = trim(line);

        if (trimmedLine.empty() || trimmedLine[0] == '#') {
            continue; // Skip empty lines and comments
        }

        if (!inServerBlock) {
            if (trimmedLine == "server {") {
                inServerBlock = true;
            }
            continue;
        }

        if (trimmedLine == "}") {
            inServerBlock = false;
            continue; // End of server block
        }

        std::stringstream ss(trimmedLine);
        std::string directive;
        ss >> directive;

        if (directive == "listen") {
            ss >> _port;
        } else if (directive == "server_name") {
            ss >> _serverName;
            if (!_serverName.empty() && _serverName[_serverName.length() - 1] == ';') {
                _serverName.erase(_serverName.length() - 1);
            }
        } else if (directive == "root") {
            ss >> _root;
            if (!_root.empty() && _root[_root.length() - 1] == ';') {
                _root.erase(_root.length() - 1);
            }
        } else if (directive == "error_page") {
            int code;
            std::string page;
            ss >> code >> page;
            _errorPages[code] = page;
        }
        // Note: location blocks are ignored for now

        // Check for trailing semicolon, as is common
        std::string lastPart;
        ss >> lastPart;
        if (lastPart.empty() || lastPart[lastPart.size() - 1] != ';') {
             // Simple check, can be improved
        }
    }

    if (_port == 0) {
        throw std::runtime_error("Port not specified in config file.");
    }
}

int ConfigParser::getPort() const {
    return _port;
}

const std::string& ConfigParser::getServerName() const {
    return _serverName;
}

const std::string& ConfigParser::getRoot() const {
    return _root;
}

const std::string& ConfigParser::getErrorPage(int errorCode) const {
    std::map<int, std::string>::const_iterator it = _errorPages.find(errorCode);
    if (it != _errorPages.end()) {
        return it->second;
    }
    return _defaultErrorPage; // Return a default if specific code not found
}
