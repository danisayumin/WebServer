#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"

class HttpRequestParser; // Forward declaration
struct LocationConfig;    // Forward declaration for LocationConfig (changed to struct)

class ClientConnection {
public:
    ClientConnection(int fd);
    ~ClientConnection();

    int getFd() const;
    ssize_t readRequest();
    bool isRequestComplete() const;
    const HttpRequest& getRequest() const;
    size_t getRequestBufferSize() const; // Re-add this declaration
    void replaceParser();
    void setResponse(const std::string& response);
    const std::string& getResponseBuffer() const;
    void clearResponseBuffer();

    // For CGI
    void setCgiPid(pid_t pid);
    pid_t getCgiPid() const;
    void setCgiPipeFd(int fd);
    int getCgiPipeFd() const;
    void setCgiLocation(const LocationConfig* loc); // Forward declare LocationConfig
    const LocationConfig* getCgiLocation() const;

private:
    int _fd;
    std::string _requestBuffer;
    std::string _responseBuffer;
    HttpRequestParser* _parser; // Use pointer
    pid_t _cgiPid;
    int _cgiPipeFd;
    const LocationConfig* _cgiLocation;
};

#endif // CLIENT_CONNECTION_HPP

