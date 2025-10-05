#include "Server.hpp"
#include "ClientConnection.hpp"

// A definição de ServerConfig agora está em Server.hpp

// --- Implementação da Classe Server ---

// 2. Implementa o construtor para inicializar os atributos
Server::Server() : _max_fd(0) {
    // Limpa os conjuntos de file descriptors
    FD_ZERO(&_master_set);
    FD_ZERO(&_read_fds);
    FD_ZERO(&_write_fds);
    // O construtor está pronto. O resto da inicialização ocorre em init().
}

Server::~Server() {
    // Percorre todos os sockets de escuta e os fecha
    for (std::map<int, ServerConfig>::iterator it = _listening_sockets.begin(); it != _listening_sockets.end(); ++it) {
        close(it->first);
    }
    // Fecha todos os sockets de cliente restantes e deleta os objetos ClientConnection
    for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first); // Fecha o file descriptor
        delete it->second; // Deleta o objeto alocado dinamicamente
    }
}

// Método privado para configurar um único socket de servidor
int setupServerSocket(int port) {
    // ... (código de setupServerSocket inalterado)
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
        throw std::runtime_error("Failed to bind socket to port");
    }

    if (listen(listen_fd, SOMAXCONN) < 0) {
        close(listen_fd);
        throw std::runtime_error("Failed to listen on socket");
    }

    std::cout << "Server listening on port " << port << std::endl;
    return listen_fd;
}

void Server::init(const char* configFile) {
    (void)configFile;
    int port = 8080;
    int listen_fd = setupServerSocket(port);
    FD_SET(listen_fd, &_master_set);
    if (listen_fd > _max_fd) {
        _max_fd = listen_fd;
    }
    _listening_sockets[listen_fd] = ServerConfig();
}

void Server::run() {
    std::cout << "Server ready. Waiting for connections..." << std::endl;
    while (true) {
        _read_fds = _master_set;
        int ready_count = select(_max_fd + 1, &_read_fds, NULL, NULL, NULL);
        if (ready_count < 0) {
            std::cerr << "Error in select()" << std::endl;
            continue;
        }
        for (int fd = 0; fd <= _max_fd; ++fd) {
            if (FD_ISSET(fd, &_read_fds)) {
                if (_listening_sockets.count(fd)) {
                    acceptNewConnection(fd);
                } else {
                    handleClientRequest(fd);
                }
            }
        }
    }
}

void Server::acceptNewConnection(int listening_fd) {
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd = accept(listening_fd, (struct sockaddr*)&client_addr, &addr_len);

    if (client_fd < 0) {
        std::cerr << "Error: accept() failed" << std::endl;
        return;
    }

    if (fcntl(client_fd, F_SETFL, O_NONBLOCK) < 0) {
        std::cerr << "Error: fcntl() failed on client socket" << std::endl;
        close(client_fd);
        return;
    }

    FD_SET(client_fd, &_master_set);
    // Aloca um novo objeto ClientConnection no heap e armazena o ponteiro no mapa
    _clients[client_fd] = new ClientConnection(client_fd);

    if (client_fd > _max_fd) {
        _max_fd = client_fd;
    }

    std::cout << "New connection accepted on fd " << client_fd << std::endl;
}

void Server::handleClientRequest(int client_fd) {
    // Usa o ponteiro para chamar o método
    ssize_t bytes_read = _clients[client_fd]->readRequest();

    if (bytes_read > 0) {
        std::cout << bytes_read << " bytes read from client " << client_fd << std::endl;
    } else {
        if (bytes_read == 0) {
            std::cout << "Client " << client_fd << " disconnected." << std::endl;
        } else {
            std::cerr << "Error reading from client " << client_fd << std::endl;
        }

        close(client_fd);
        FD_CLR(client_fd, &_master_set);
        // Deleta o objeto ClientConnection para liberar memória
        delete _clients[client_fd];
        _clients.erase(client_fd);
    }
}
