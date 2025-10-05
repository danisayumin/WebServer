#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <vector>
#include <string>

// Para ssize_t (retorno do read)
#include <unistd.h>

#ifndef CLIENTCONNECTION_HPP
#define CLIENTCONNECTION_HPP

#include <string>
#include <unistd.h> // Para ssize_t

class ClientConnection {
public:
    explicit ClientConnection(int client_fd);
    ~ClientConnection();

    int getFd() const;
    ssize_t readRequest();
    const std::string& getRequestBuffer() const;
    bool isRequestComplete() const;

private:
    int _client_fd;
    std::string _requestBuffer;
    bool _is_request_complete;
};

#endif // CLIENTCONNECTION_HPP

#endif // CLIENT_CONNECTION_HPP
