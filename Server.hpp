#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/select.h>
#include <map>
#include <string>
#include <vector>
#include "ConfigParser.hpp"
#include "ClientConnection.hpp"
#include "ServerConfig.hpp"

class Server {
public:
    Server(const ConfigParser& config);
    ~Server();

    void run();

private:
    void _acceptNewConnection(int listening_fd);
    void _handleClientData(int client_fd);
    void _handleClientWrite(int client_fd);
    void _handleCgiRead(int pipe_fd);
    void _executeCgi(ClientConnection* client, const LocationConfig* loc);
    void _handleCgiWrite(int pipe_fd);
    void _sendErrorResponse(ClientConnection* client, int code, const std::string& message, const ServerConfig* server_config, const LocationConfig* loc);
    int _setupServerSocket(int port);
    const ServerConfig* _findServerConfig(int port, const std::string& host_header) const;

    std::map<int, std::vector<ServerConfig*> > _configs_by_port;
    std::map<int, int> _port_by_listen_fd;
    
    std::vector<int> _listen_fds;
    int _max_fd;
    fd_set _master_set;
    fd_set _write_fds;
    std::map<int, ClientConnection*> _clients;
    std::map<int, int> _pipe_to_client_map;
    std::map<int, int> _cgi_stdin_pipe_to_client_map;
};

#endif
