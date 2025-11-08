#include "ClientConnection.hpp"
#include "HttpRequest.hpp" // Needed for parsing headers
#include <unistd.h> // Para read
#include <iostream>
#include <cstdlib> // para strtol

ClientConnection::ClientConnection(int client_fd) :
    _client_fd(client_fd),
    _headers_received(false),
    _content_length(0),
    _body_bytes_read(0),
    _cgi_pid(0),
    _cgi_pipe_fd(-1)
{
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
    char buffer[4096];
    ssize_t bytes_read = read(_client_fd, buffer, sizeof(buffer) - 1);

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        _requestBuffer.append(buffer, bytes_read);

        if (!_headers_received) {
            size_t headers_end_pos = _requestBuffer.find("\r\n\r\n");
            if (headers_end_pos != std::string::npos) {
                _headers_received = true;
                
                // Manually parse Content-Length
                size_t cl_pos = _requestBuffer.find("Content-Length: ");
                if (cl_pos != std::string::npos) {
                    cl_pos += 16; // length of "Content-Length: "
                    size_t cl_end_pos = _requestBuffer.find("\r\n", cl_pos);
                    if (cl_end_pos != std::string::npos) {
                        std::string cl_value = _requestBuffer.substr(cl_pos, cl_end_pos - cl_pos);
                        _content_length = std::strtol(cl_value.c_str(), NULL, 10);
                    }
                }
                
                size_t body_start_pos = headers_end_pos + 4;
                _body_bytes_read = _requestBuffer.length() - body_start_pos;
            }
        } else {
            _body_bytes_read += bytes_read;
        }
    }
    return bytes_read;
}
// Retorna o buffer com os dados da requisição
const std::string& ClientConnection::getRequestBuffer() const {
    return _requestBuffer;
}

bool ClientConnection::isRequestComplete() const {
    if (!_headers_received) {
        return false;
    }
    return _body_bytes_read >= _content_length;
}

void ClientConnection::setResponse(const std::string& response) {
    _responseBuffer = response;
}

const std::string& ClientConnection::getResponseBuffer() const {
    return _responseBuffer;
}

void ClientConnection::clearResponseBuffer() {
    _responseBuffer.clear();
}

// CGI-related methods
void ClientConnection::setCgiPid(pid_t pid) {
    _cgi_pid = pid;
}

pid_t ClientConnection::getCgiPid() const {
    return _cgi_pid;
}

void ClientConnection::setCgiPipeFd(int fd) {
    _cgi_pipe_fd = fd;
}

int ClientConnection::getCgiPipeFd() const {
    return _cgi_pipe_fd;
}
