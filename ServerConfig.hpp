#ifndef SERVERCONFIG_HPP
#define SERVERCONFIG_HPP

#include <string>
#include <vector>
#include <map>
#include "LocationConfig.hpp"

struct ServerConfig {
    std::vector<int> ports;
    std::vector<std::string> server_names;
    std::string root;
    std::map<int, std::string> error_pages;
    size_t client_max_body_size;
    std::vector<LocationConfig*> locations;

    ServerConfig() : client_max_body_size(1 * 1024 * 1024) {} // Default 1MB

    // Destructor to clean up allocated LocationConfig objects
    ~ServerConfig() {
        for (size_t i = 0; i < locations.size(); ++i) {
            delete locations[i];
        }
    }
};

#endif
