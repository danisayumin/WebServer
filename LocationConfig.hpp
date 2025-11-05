#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    std::string cgi_path;
    std::string cgi_ext;
    // Mais diretivas no futuro
};

#endif
