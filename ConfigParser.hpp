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

    int getPort() const;
    const std::string& getRoot() const;
    const std::vector<LocationConfig*>& getLocations() const;

private:
    void parse();

    std::string _filePath;
    int _port;
    std::string _root;
    std::vector<LocationConfig*> _locations;
};

#endif