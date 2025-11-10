#ifndef LOCATION_CONFIG_HPP
#define LOCATION_CONFIG_HPP

#include <string>
#include <map> // Added for std::map

struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    std::string cgi_path;
    std::string cgi_ext;
    size_t client_max_body_size;
    std::map<int, std::string> error_pages;
    std::vector<std::string> allowed_methods;
    std::string redirect; // New member for HTTP redirection
    std::string upload_path; // New member for upload directory
    bool autoindex; // New member for directory listing

    LocationConfig() : client_max_body_size(1 * 1024 * 1024), autoindex(false) {} // Default 1MB, autoindex off
};

#endif
