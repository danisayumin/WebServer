#ifndef LOCATIONCONFIG_HPP
#define LOCATIONCONFIG_HPP

#include <string>
#include <vector>

struct LocationConfig {
    std::string path;
    std::string root;
    std::string index;
    std::vector<std::string> cgi_pass;
    // Mais diretivas no futuro
};

#endif
