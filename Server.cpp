#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <stdexcept>
#include <sstream>
#include <fstream>

// Função auxiliar para pegar o MIME Type com base na extensão do arquivo
std::string getMimeType(const std::string& filePath) {
    if (filePath.rfind(".html") != std::string::npos) return "text/html";
    if (filePath.rfind(".css") != std::string::npos) return "text/css";
    if (filePath.rfind(".js") != std::string::npos) return "application/javascript";
    if (filePath.rfind(".jpg") != std::string::npos) return "image/jpeg";
    if (filePath.rfind(".png") != std::string::npos) return "image/png";
    return "application/octet-stream"; // Default binary type
}

Server::Server(const ConfigParser& config) : _max_fd(0) {
    FD_ZERO(&_master_set);
    FD_ZERO(&_read_fds);
    FD_ZERO(&_write_fds);

    try {
        int listen_fd = _setupServerSocket(config.getPort());
        _listening_sockets.insert(std::make_pair(listen_fd, config)); // Associa o fd com sua config
    } catch (const std::exception& e) {
        std::cerr << "Server setup failed: " << e.what() << std::endl;
        throw;
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

int Server::_setupServerSocket(int port) {
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
    std::cout << "Server listening on port " << port << std::endl;
    return listen_fd;
}

void Server::run() {
    std::cout << "Server ready. Waiting for connections..." << std::endl;
    while (true) {
        _read_fds = _master_set;
        if (select(_max_fd + 1, &_read_fds, NULL, NULL, NULL) < 0) {
            perror("select");
            continue;
        }

        for (int fd = 0; fd <= _max_fd; ++fd) {
            if (FD_ISSET(fd, &_read_fds)) {
                if (_listening_sockets.count(fd)) {
                    _acceptNewConnection(fd);
                } else if (_clients.count(fd)) {
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
    ClientConnection* client = _clients[client_fd];
    ssize_t bytes_read = client->readRequest();

    if (bytes_read > 0) {
        if (client->isRequestComplete()) {
            HttpRequest req(client->getRequestBuffer());
            HttpResponse res;

            if (req.getMethod() != "GET") {
                res.setStatusCode(405, "Method Not Allowed");
                res.addHeader("Allow", "GET");
                res.addHeader("Content-Length", "0");
                res.setBody("");
            } else {
                // Lógica para GET
                std::string root = _listening_sockets.begin()->second.getRoot();
                std::string uri = req.getUri();
                if (uri == "/") {
                    uri = "/index.html";
                }

                std::string filePath = root + uri;

                std::ifstream file(filePath.c_str());
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string body = buffer.str();

                    res.setStatusCode(200, "OK");
                    res.addHeader("Content-Type", getMimeType(filePath));
                    std::stringstream ss_len;
                    ss_len << body.length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(body);
                } else {
                    std::string body = "<html><body><h1>404 Not Found</h1></body></html>";
                    res.setStatusCode(404, "Not Found");
                    res.addHeader("Content-Type", "text/html");
                    std::stringstream ss_len;
                    ss_len << body.length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(body);
                }
            }

            std::string responseStr = res.toString();
            send(client_fd, responseStr.c_str(), responseStr.length(), 0);

            close(client_fd);
            FD_CLR(client_fd, &_master_set);
            delete _clients[client_fd];
            _clients.erase(client_fd);
        }
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