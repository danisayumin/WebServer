#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <iostream>
#include <stdexcept>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sstream>
#include <fstream>

std::string getMimeType(const std::string& filePath) {
    if (filePath.rfind(".html") != std::string::npos) return "text/html";
    if (filePath.rfind(".css") != std::string::npos) return "text/css";
    return "application/octet-stream";
}

Server::Server(const ConfigParser& config) : _config(config), _listen_fd(0), _max_fd(0) {
    FD_ZERO(&_master_set);
    _setupServerSocket();
}

Server::~Server() {
    if (_listen_fd > 0) close(_listen_fd);
    for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
}

void Server::_setupServerSocket() {
    _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_listen_fd < 0) throw std::runtime_error("socket() failed");

    int on = 1;
    if (setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        close(_listen_fd);
        throw std::runtime_error("setsockopt() failed");
    }
    if (fcntl(_listen_fd, F_SETFL, O_NONBLOCK) < 0) {
        close(_listen_fd);
        throw std::runtime_error("fcntl() failed");
    }

    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(_config.getPort());

    if (bind(_listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(_listen_fd);
        throw std::runtime_error("bind() failed");
    }
    if (listen(_listen_fd, SOMAXCONN) < 0) {
        close(_listen_fd);
        throw std::runtime_error("listen() failed");
    }

    FD_SET(_listen_fd, &_master_set);
    _max_fd = _listen_fd;
    std::cout << "Server listening on port " << _config.getPort() << std::endl;
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
                    _acceptNewConnection();
                } else if (_clients.count(fd)) {
                    _handleClientData(fd);
                }
            }
        }
    }
}

void Server::_acceptNewConnection() {
    int client_fd = accept(_listen_fd, NULL, NULL);
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
                const std::vector<LocationConfig*>& locations = _config.getLocations();
                const LocationConfig* best_match = NULL;
                size_t longest_match = 0;

                for (size_t i = 0; i < locations.size(); ++i) {
                    if (req.getUri().find(locations[i]->path) == 0) {
                        if (locations[i]->path.length() > longest_match) {
                            longest_match = locations[i]->path.length();
                            best_match = locations[i];
                        }
                    }
                }

                std::string root = _config.getRoot();
                std::string uri = req.getUri();

                if (best_match) {
                    if (!best_match->root.empty()) {
                        root = best_match->root;
                    }
                    if (uri == best_match->path && !best_match->index.empty()) {
                        uri = "/" + best_match->index; // Adiciona a barra que faltava
                    }
                }
                
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
                    res.addHeader("Content-Type", "text/html");
                    std::string body = "<html><body><h1>404 Not Found</h1></body></html>";
                    std::stringstream ss_len; ss_len << body.length();
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
        close(client_fd);
        FD_CLR(client_fd, &_master_set);
        delete _clients[client_fd];
        _clients.erase(client_fd);
    }
}