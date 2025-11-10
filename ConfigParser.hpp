#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

class ConfigParser {
public:
    ConfigParser(const std::string& filePath);
    ~ConfigParser();

    const std::vector<int>& getPorts() const;
    const std::string& getRoot() const;
    const std::vector<LocationConfig*>& getLocations() const;
    const std::map<int, std::string>& getErrorPages() const;

private:
    void parse();
    size_t _parseSize(const std::string& size_str);

    std::string _filePath;
    std::vector<int> _ports;
    std::string _root;
    std::vector<LocationConfig*> _locations;
    std::map<int, std::string> _error_pages;
};

#endif