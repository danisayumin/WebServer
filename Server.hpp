#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/select.h>
#include <map>
#include <string>
#include <ctime>
#include "ConfigParser.hpp"
#include "ClientConnection.hpp"

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
    void _sendErrorResponse(ClientConnection* client, int code, const std::string& message, const LocationConfig* loc);
    int _setupServerSocket(int port); // Helper to setup a single socket
    void _checkCgiTimeouts(); // Check for CGI timeouts

    const ConfigParser& _config;
    std::vector<int> _listen_fds; // Changed to vector
    int _max_fd;
    fd_set _master_set;
    fd_set _write_fds;
    std::map<int, ClientConnection*> _clients;
    std::map<int, int> _pipe_to_client_map; // Maps CGI stdout pipe READ_END to client_fd
    std::map<int, int> _cgi_stdin_pipe_to_client_map; // Maps CGI stdin pipe WRITE_END to client_fd
    std::map<int, time_t> _cgi_start_times; // Maps client_fd to CGI start time
    std::map<int, int> _cgi_timeouts; // Maps client_fd to CGI timeout in seconds
};

#endif
