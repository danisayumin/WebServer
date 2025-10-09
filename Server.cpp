#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <algorithm>

std::string getMimeType(const std::string& filePath) {
    if (filePath.rfind(".html") != std::string::npos) return "text/html";
    if (filePath.rfind(".css") != std::string::npos) return "text/css";
    return "application/octet-stream";
}

Server::Server(const ConfigParser& config) : _config(config), _listen_fd(0), _max_fd(0) {
    FD_ZERO(&_master_set);
    FD_ZERO(&_read_fds);
    FD_ZERO(&_write_fds);
    _listen_fd = _setupServerSocket(_config.getPort());
    _max_fd = _listen_fd;
}

Server::~Server() {
    if (_listen_fd > 0) close(_listen_fd);
    for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
}

int Server::_setupServerSocket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt() failed");
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        throw std::runtime_error("fcntl() failed");
    }
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(fd);
        throw std::runtime_error("bind() failed");
    }
    if (listen(fd, SOMAXCONN) < 0) {
        close(fd);
        throw std::runtime_error("listen() failed");
    }
    FD_SET(fd, &_master_set);
    std::cout << "Server listening on port " << port << std::endl;
    return fd;
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
                if (fd == _listen_fd) {
                    _acceptNewConnection(fd);
                } else if (_clients.count(fd)) {
                    _handleClientData(fd);
                }
            }
        }
    }
}

void Server::_acceptNewConnection(int listening_fd) {
    int client_fd = accept(listening_fd, NULL, NULL);
    if (client_fd < 0) return;
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    FD_SET(client_fd, &_master_set);
    _clients[client_fd] = new ClientConnection(client_fd);
    if (client_fd > _max_fd) _max_fd = client_fd;
    std::cout << "New connection: " << client_fd << std::endl;
}

void Server::_handleClientData(int client_fd) {
    ClientConnection* client = _clients[client_fd];
    if (client->readRequest() > 0) {
        if (client->isRequestComplete()) {
            HttpRequest req(client->getRequestBuffer());
            HttpResponse res;
            if (req.getMethod() != "GET") {
                res.setStatusCode(405, "Method Not Allowed");
            } else {
                std::string root = _config.getRoot();
                std::string uri = req.getUri();
                if (uri == "/") uri = "/index.html";
                std::string filePath = root + uri;
                std::ifstream file(filePath.c_str());
                if (file.is_open()) {
                    std::stringstream buffer; buffer << file.rdbuf();
                    res.setStatusCode(200, "OK");
                    res.addHeader("Content-Type", getMimeType(filePath));
                    std::stringstream ss_len; ss_len << buffer.str().length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(buffer.str());
                } else {
                    res.setStatusCode(404, "Not Found");
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
        close(client_fd);
        FD_CLR(client_fd, &_master_set);
        delete _clients[client_fd];
        _clients.erase(client_fd);
    }
}

