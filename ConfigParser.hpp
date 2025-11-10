#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <vector>
#include "ServerConfig.hpp"

class ConfigParser {
public:
    ConfigParser(const std::string& filePath);
    ~ConfigParser();

    const std::vector<ServerConfig*>& getServerConfigs() const;

private:
    void parse();
    size_t _parseSize(const std::string& size_str);

    std::string _filePath;
    std::vector<ServerConfig*> _server_configs;
};

#endif