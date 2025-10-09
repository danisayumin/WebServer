#ifndef SERVER_HPP
#define SERVER_HPP

#include <map>
#include <string>
#include "ConfigParser.hpp"
#include "ClientConnection.hpp"

class Server {
public:
    explicit Server(const ConfigParser& config);
    ~Server();
    void run();

private:
    void _acceptNewConnection();
    void _handleClientData(int client_fd);
    void _executeCgi(ClientConnection* client, const LocationConfig* loc);
    void _setupServerSocket();

    const ConfigParser& _config;
    int _listen_fd;
    int _max_fd;
    fd_set _master_set;
    fd_set _read_fds;
    std::map<int, ClientConnection*> _clients;
};

#endif
