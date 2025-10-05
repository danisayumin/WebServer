#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <vector>
#include <map>

class ConfigParser {
public:
    // Constructor that takes the path to the configuration file
    ConfigParser(const std::string& filePath);
    ~ConfigParser();

    // Getter methods for the parsed configuration
    int getPort() const;
    const std::string& getServerName() const;
    const std::string& getRoot() const;
    const std::string& getErrorPage(int errorCode) const;

private:
    // Private method to handle the parsing logic
    void parse();

    std::string _filePath;
    int _port;
    std::string _serverName;
    std::string _root;
    std::map<int, std::string> _errorPages;
    std::string _defaultErrorPage; // Fallback for unregistered error codes
};

#endif // CONFIGPARSER_HPP