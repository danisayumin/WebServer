#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <exception>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>

#include "ConfigParser.hpp"
#include "ClientConnection.hpp"

class Server {
public:
    explicit Server(const ConfigParser& config);
    ~Server();

    void run();

private:
    void _setupServerSocket(int port);
    void _acceptNewConnection(int listening_fd);
    void _handleClientData(int client_fd);

    std::map<int, ConfigParser> _listening_sockets;
    std::map<int, ClientConnection*> _clients;

    fd_set _master_set;
    fd_set _read_fds;
    fd_set _write_fds;

    int _max_fd;
};

#endif // SERVER_HPP
