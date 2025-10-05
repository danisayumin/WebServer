#include "ClientConnection.hpp"
#include <iostream>

#define BUFFER_SIZE 2048

ClientConnection::ClientConnection(int client_fd) : _fd(client_fd) {
    // Aloca um buffer de tamanho fixo para a leitura inicial
    _read_buffer.resize(BUFFER_SIZE);
    std::cout << "New client connection created for fd: " << _fd << std::endl;
}

ClientConnection::~ClientConnection() {
    std::cout << "Client connection destroyed for fd: " << _fd << std::endl;
}

ssize_t ClientConnection::readRequest() {
    // Tenta ler dados do socket para o buffer
    // A função 'read' é uma chamada de sistema direta
    return read(_fd, &_read_buffer[0], _read_buffer.size());
}

int ClientConnection::getFd() const {
    return _fd;
}

// Implementações vazias para o construtor de cópia e operador de atribuição
ClientConnection::ClientConnection(const ClientConnection& other) {
    (void)other;
}

ClientConnection& ClientConnection::operator=(const ClientConnection& other) {
    (void)other;
    return *this;
}
