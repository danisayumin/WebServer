#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <string>
#include <sys/types.h> // Para pid_t
#include <unistd.h>    // Para ssize_t

class ClientConnection {
public:
    explicit ClientConnection(int client_fd);
    ~ClientConnection();

    int getFd() const;
    ssize_t readRequest();
    const std::string& getRequestBuffer() const;
    bool isRequestComplete() const;

    void setResponse(const std::string& response);
    const std::string& getResponseBuffer() const;
    void clearResponseBuffer();

    // CGI-related methods
    void setCgiPid(pid_t pid);
    pid_t getCgiPid() const;
    void setCgiPipeFd(int fd);
    int getCgiPipeFd() const;

private:
    int _client_fd;
    std::string _requestBuffer;
    std::string _responseBuffer;

    // Request parsing state
    bool _headers_received;
    size_t _content_length;
    size_t _body_bytes_read;

    // CGI-related members
    pid_t _cgi_pid;
    int _cgi_pipe_fd;
};

#endif // CLIENT_CONNECTION_HPP
