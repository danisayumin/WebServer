#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/select.h>
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
    void _acceptNewConnection(int listening_fd);
    void _handleClientData(int client_fd);
    void _handleClientWrite(int client_fd);
    void _handleCgiRead(int pipe_fd);
    void _executeCgi(ClientConnection* client, const LocationConfig* loc);
    int _setupServerSocket(int port);

    const ConfigParser& _config;
    int _listen_fd;
    int _max_fd;
    fd_set _master_set;
    fd_set _write_fds;
    std::map<int, ClientConnection*> _clients;
    std::map<int, int> _pipe_to_client_map; // pipe_fd -> client_fd
};

#endif
