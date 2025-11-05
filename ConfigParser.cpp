#include "ConfigParser.hpp"
#include "LocationConfig.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstdlib>

static std::string trim(const std::string& s) {
    const std::string whitespace = " \t\n\r";
    size_t start = s.find_first_not_of(whitespace);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(whitespace);
    return s.substr(start, end - start + 1);
}

ConfigParser::ConfigParser(const std::string& filePath) : _filePath(filePath), _port(0) {
    parse();
}

ConfigParser::~ConfigParser() {
    for (size_t i = 0; i < _locations.size(); ++i) {
        delete _locations[i];
    }
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
        } else {
            if (directive == "listen") _port = std::atoi(value.c_str());
            else if (directive == "root") _root = value;
        }
    }
    if (_port == 0) throw std::runtime_error("Port not specified");
}

int ConfigParser::getPort() const { return _port; }
const std::string& ConfigParser::getRoot() const { return _root; }
const std::vector<LocationConfig*>& ConfigParser::getLocations() const { return _locations; }

