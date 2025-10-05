#include "Server.hpp"
#include <stdexcept>

Server::Server(const ConfigParser& config) : _max_fd(0) {
    FD_ZERO(&_master_set);
    FD_ZERO(&_read_fds);
    FD_ZERO(&_write_fds);

    try {
        _setupServerSocket(config.getPort());
    } catch (const std::exception& e) {
        std::cerr << "Server setup failed: " << e.what() << std::endl;
        // Em um caso real, o destrutor deveria limpar os sockets já abertos.
        throw; // Re-lança a exceção para terminar o programa se o setup falhar.
    }
}

Server::~Server() {
    for (std::map<int, ConfigParser>::iterator it = _listening_sockets.begin(); it != _listening_sockets.end(); ++it) {
        close(it->first);
    }
    for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
}

void Server::_setupServerSocket(int port) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) throw std::runtime_error("Failed to create socket");

    int on = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to set socket options");
    }

    if (fcntl(listen_fd, F_SETFL, O_NONBLOCK) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to set socket to non-blocking");
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(listen_fd);
        std::stringstream ss;
        ss << "Failed to bind to port " << port;
        throw std::runtime_error(ss.str());
    }

    if (listen(listen_fd, SOMAXCONN) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to listen on socket");
    }

    FD_SET(listen_fd, &_master_set);
    _max_fd = listen_fd;
    // _listening_sockets[listen_fd] = config; // Store config for this socket
    std::cout << "Server listening on port " << port << std::endl;
}

void Server::run() {
    std::cout << "Server ready. Waiting for connections..." << std::endl;
    while (true) {
        _read_fds = _master_set;
        // _write_fds = _master_set; // Para escritas não-bloqueantes no futuro

        if (select(_max_fd + 1, &_read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            continue;
        }

        for (int fd = 0; fd <= _max_fd; ++fd) {
            if (FD_ISSET(fd, &_read_fds)) {
                if (_listening_sockets.count(fd)) {
                    _acceptNewConnection(fd);
                } else {
                    _handleClientData(fd);
                }
            }
        }
    }
}

void Server::_acceptNewConnection(int listening_fd) {
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listening_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (client_fd < 0) {
        perror("accept");
        return;
    }

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    FD_SET(client_fd, &_master_set);
    _clients[client_fd] = new ClientConnection(client_fd);
    if (client_fd > _max_fd) {
        _max_fd = client_fd;
    }

    std::cout << "New connection accepted on fd " << client_fd << std::endl;
}

void Server::_handleClientData(int client_fd) {
    ssize_t bytes_read = _clients[client_fd]->readRequest();

    if (bytes_read > 0) {
        std::cout << bytes_read << " bytes read from client " << client_fd << std::endl;
        // Aqui virá a lógica de parsing e resposta
    } else {
        if (bytes_read == 0) {
            std::cout << "Client " << client_fd << " disconnected." << std::endl;
        } else {
            perror("recv");
        }
        close(client_fd);
        FD_CLR(client_fd, &_master_set);
        delete _clients[client_fd];
        _clients.erase(client_fd);
    }
}
