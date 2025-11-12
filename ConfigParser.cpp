#include "ConfigParser.hpp"
#include "LocationConfig.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <cctype>

static std::string trim(const std::string& s) {
    const std::string whitespace = " \t\n\r";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

ConfigParser::ConfigParser(const std::string& filePath) : _filePath(filePath), _root("./www") {
    parse();
}

ConfigParser::~ConfigParser() {
    for (size_t i = 0; i < _locations.size(); ++i) {
        delete _locations[i];
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
        if (suffix == 'K') {
            return num_val * 1024;
        } else if (suffix == 'M') {
            return num_val * 1024 * 1024;
        } else if (suffix == 'G') {
            return num_val * 1024 * 1024 * 1024;
        }
    }
    return num_val;
}

void ConfigParser::parse() {
    std::ifstream configFile(_filePath.c_str());
    if (!configFile.is_open()) throw std::runtime_error("Could not open file");

    std::string line;
    bool in_server_block = false;
    LocationConfig* current_location = NULL;

    while (std::getline(configFile, line)) {
        std::string trimmedLine = trim(line);
        if (trimmedLine.empty() || trimmedLine[0] == '#') continue;

        if (!in_server_block) {
            if (trimmedLine == "server {") in_server_block = true;
            continue;
        }

        if (trimmedLine == "}" && !current_location) {
            in_server_block = false;
            continue;
        }

        if (trimmedLine.rfind("location", 0) == 0) {
            std::stringstream ss(trimmedLine);
            std::string keyword, path;
            ss >> keyword >> path;
            LocationConfig* new_loc = new LocationConfig();
            new_loc->path = path;
            _locations.push_back(new_loc);
            current_location = new_loc;
            continue;
        }

        if (trimmedLine == "}" && current_location) {
            current_location = NULL;
            continue;
        }

        std::stringstream ss(trimmedLine);
        std::string directive, value;
        ss >> directive;
        if (ss >> value && !value.empty() && value[value.length() - 1] == ';') {
            value.erase(value.length() - 1);
        }

        if (current_location) {
            if (directive == "root") current_location->root = value;
            else if (directive == "index") current_location->index = value;
            else if (directive == "cgi_path") current_location->cgi_path = value;
            else if (directive == "cgi_ext") current_location->cgi_ext = value;
            else if (directive == "cgi_timeout") current_location->cgi_timeout = std::atoi(value.c_str());
            else if (directive == "client_max_body_size") current_location->client_max_body_size = _parseSize(value);
            else if (directive == "error_page") {
                std::stringstream value_ss(trimmedLine);
                std::string temp_directive;
                int error_code;
                std::string error_path;
                value_ss >> temp_directive >> error_code >> error_path;
                if (!error_path.empty() && error_path[error_path.length() - 1] == ';') {
                    error_path.erase(error_path.length() - 1);
                }
                current_location->error_pages[error_code] = error_path;
            } else if (directive == "allow_methods") {
                std::stringstream value_ss(trimmedLine);
                std::string temp_directive;
                std::string method;
                value_ss >> temp_directive; // consume "allow_methods"
                while (value_ss >> method) {
                    if (!method.empty() && method[method.length() - 1] == ';') {
                        method.erase(method.length() - 1);
                    }
                    current_location->allowed_methods.push_back(method);
                }
            } else if (directive == "redirect") {
                current_location->redirect = value;
            } else if (directive == "upload_path") {
                current_location->upload_path = value;
            } else if (directive == "autoindex") {
                if (value == "on") {
                    current_location->autoindex = true;
                } else if (value == "off") {
                    current_location->autoindex = false;
                } else {
                    throw std::runtime_error("Invalid value for autoindex. Use 'on' or 'off'.");
                }
            }
        } else {
            if (directive == "listen") _ports.push_back(std::atoi(value.c_str()));
            else if (directive == "root") _root = value;
            else if (directive == "error_page") {
                std::stringstream value_ss(trimmedLine);
                std::string temp_directive;
                int error_code;
                std::string error_path;
                value_ss >> temp_directive >> error_code >> error_path;
                if (!error_path.empty() && error_path[error_path.length() - 1] == ';') {
                    error_path.erase(error_path.length() - 1);
                }
                _error_pages[error_code] = error_path;
            }
        }
    }
    if (_ports.empty()) throw std::runtime_error("Port not specified");
}

const std::vector<int>& ConfigParser::getPorts() const { return _ports; }
const std::string& ConfigParser::getRoot() const { return _root; }
const std::vector<LocationConfig*>& ConfigParser::getLocations() const { return _locations; }
const std::map<int, std::string>& ConfigParser::getErrorPages() const { return _error_pages; }
