#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class HttpRequestParser;
struct LocationConfig;
struct ServerConfig; // Forward declaration

class ClientConnection {
public:
    ClientConnection(int fd, int port);
    ~ClientConnection();

    int getFd() const;
    int getPort() const;
    ssize_t readRequest();
    bool isRequestComplete() const;
    const HttpRequest& getRequest() const;
    size_t getRequestBufferSize() const;
    void replaceParser();
    void setResponse(const std::string& response);
    const std::string& getResponseBuffer() const;
    void clearResponseBuffer();

    // Config context
    void setServerConfig(const ServerConfig* sc);
    const ServerConfig* getServerConfig() const;
    void setLocationConfig(const LocationConfig* lc);
    const LocationConfig* getLocationConfig() const;

    // For CGI
    void setCgiPid(pid_t pid);
    pid_t getCgiPid() const;
    void setCgiPipeFd(int fd);
    int getCgiPipeFd() const;

private:
    int _fd;
    int _port;
    HttpRequestParser* _parser;
    std::string _responseBuffer;

    // Request context
    const ServerConfig* _server_config;
    const LocationConfig* _location_config;

    // CGI state
    pid_t _cgiPid;
    int _cgiPipeFd;
};

#endif // CLIENT_CONNECTION_HPP

