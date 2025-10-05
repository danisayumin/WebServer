#include "ClientConnection.hpp"
#include <unistd.h> // Para read
#include <iostream>

ClientConnection::ClientConnection(int client_fd) : _client_fd(client_fd), _is_request_complete(false) {
    // Construtor
}

ClientConnection::~ClientConnection() {
    // Destrutor
}

int ClientConnection::getFd() const {
    return _client_fd;
}

// Lê dados do socket e anexa ao buffer interno
ssize_t ClientConnection::readRequest() {
    char buffer[1024];
    ssize_t bytes_read = read(_client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        _requestBuffer.append(buffer);
        // Verifica se a requisição está completa
        if (_requestBuffer.find("\r\n\r\n") != std::string::npos) {
            _is_request_complete = true;
        }
    }
    return bytes_read;
}

// Retorna o buffer com os dados da requisição
const std::string& ClientConnection::getRequestBuffer() const {
    return _requestBuffer;
}

bool ClientConnection::isRequestComplete() const {
    return _is_request_complete;
}

