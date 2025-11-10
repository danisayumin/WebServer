#include "ConfigParser.hpp"
#include "ServerConfig.hpp"
#include "LocationConfig.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cctype>
#include <vector>

// Helper to trim whitespace from both ends of a string
static std::string trim(const std::string& s) {
    const std::string whitespace = " \t\n\r";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

// Helper to split a string by a delimiter
static std::vector<std::string> split(const std::string& s, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(s);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

ConfigParser::ConfigParser(const std::string& filePath) : _filePath(filePath) {
    parse();
}

ConfigParser::~ConfigParser() {
    for (size_t i = 0; i < _server_configs.size(); ++i) {
        delete _server_configs[i];
    }
}

size_t ConfigParser::_parseSize(const std::string& size_str) {
    if (size_str.empty()) return 0;

    char* end = NULL;
    long value = std::strtol(size_str.c_str(), &end, 10);
    if (value < 0) return 0;

    size_t num_val = static_cast<size_t>(value);

    if (*end) {
        char suffix = std::toupper(*end);
        if (suffix == 'K') return num_val * 1024;
        if (suffix == 'M') return num_val * 1024 * 1024;
        if (suffix == 'G') return num_val * 1024 * 1024 * 1024;
    }
    return num_val;
}

void ConfigParser::parse() {
    std::ifstream configFile(_filePath.c_str());
    if (!configFile.is_open()) throw std::runtime_error("Could not open config file: " + _filePath);

    std::string line;
    ServerConfig* current_server = NULL;
    LocationConfig* current_location = NULL;
    int brace_level = 0;

    while (std::getline(configFile, line)) {
        std::string trimmedLine = trim(line);
        if (trimmedLine.empty() || trimmedLine[0] == '#') continue;

        std::vector<std::string> tokens = split(trimmedLine, ' ');
        if (tokens.empty()) continue;

        std::string directive = tokens[0];

        if (directive == "server" && tokens.size() > 1 && tokens[1] == "{") {
            if (brace_level != 0) throw std::runtime_error("Syntax error: nested server blocks are not allowed.");
            current_server = new ServerConfig();
            _server_configs.push_back(current_server);
            brace_level++;
            continue;
        }

        if (directive == "location" && tokens.size() >= 3 && tokens[2] == "{") {
            if (brace_level != 1) throw std::runtime_error("Syntax error: location block must be inside a server block.");
            current_location = new LocationConfig();
            current_location->path = tokens[1];
            current_server->locations.push_back(current_location);
            brace_level++;
            continue;
        }

        if (directive == "}") {
            if (brace_level == 2) { // Closing a location block
                current_location = NULL;
            } else if (brace_level == 1) { // Closing a server block
                current_server = NULL;
            } else {
                throw std::runtime_error("Syntax error: mismatched closing brace '}'.");
            }
            brace_level--;
            continue;
        }

        // Remove trailing semicolon from the last token
        if (!tokens.back().empty() && tokens.back()[tokens.back().length() - 1] == ';') {
            tokens.back().erase(tokens.back().length() - 1);
        }

        if (brace_level == 1 && current_server) { // Server context
            if (directive == "listen" && tokens.size() > 1) {
                current_server->ports.push_back(std::atoi(tokens[1].c_str()));
            } else if (directive == "server_name" && tokens.size() > 1) {
                for (size_t i = 1; i < tokens.size(); ++i) {
                    current_server->server_names.push_back(tokens[i]);
                }
            } else if (directive == "root" && tokens.size() > 1) {
                current_server->root = tokens[1];
            } else if (directive == "client_max_body_size" && tokens.size() > 1) {
                current_server->client_max_body_size = _parseSize(tokens[1]);
            } else if (directive == "error_page" && tokens.size() > 2) {
                current_server->error_pages[std::atoi(tokens[1].c_str())] = tokens[2];
            }
        } else if (brace_level == 2 && current_location) { // Location context
            if (directive == "root" && tokens.size() > 1) current_location->root = tokens[1];
            else if (directive == "index" && tokens.size() > 1) current_location->index = tokens[1];
            else if (directive == "cgi_path" && tokens.size() > 1) current_location->cgi_path = tokens[1];
            else if (directive == "cgi_ext" && tokens.size() > 1) current_location->cgi_ext = tokens[1];
            else if (directive == "client_max_body_size" && tokens.size() > 1) current_location->client_max_body_size = _parseSize(tokens[1]);
            else if (directive == "redirect" && tokens.size() > 1) current_location->redirect = tokens[1];
            else if (directive == "upload_path" && tokens.size() > 1) current_location->upload_path = tokens[1];
            else if (directive == "autoindex" && tokens.size() > 1) current_location->autoindex = (tokens[1] == "on");
            else if (directive == "allow_methods") {
                for (size_t i = 1; i < tokens.size(); ++i) {
                    current_location->allowed_methods.push_back(tokens[i]);
                }
            } else if (directive == "error_page" && tokens.size() > 2) {
                current_location->error_pages[std::atoi(tokens[1].c_str())] = tokens[2];
            }
        }
    }

    if (brace_level != 0) {
        throw std::runtime_error("Syntax error: unclosed braces in config file.");
    }
    if (_server_configs.empty()) {
        throw std::runtime_error("No server blocks defined in config file.");
    }
}

const std::vector<ServerConfig*>& ConfigParser::getServerConfigs() const {
    return _server_configs;
}
