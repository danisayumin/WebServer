// C system headers first
#include <unistd.h> // Para read
#include <cstdlib> // para strtol
#include <cstring> // For strerror

// C++ standard library headers
#include <iostream> // For std::cerr
#include <string> // For std::string
#include <vector> // For std::vector
#include <map> // For std::map
#include <sstream> // For std::stringstream

// Project-specific headers
#include "ClientConnection.hpp"
#include "HttpRequestParser.hpp"
#include "HttpRequest.hpp"
#include "ConfigParser.hpp" // Include ConfigParser.hpp for LocationConfig definition

 ClientConnection::ClientConnection(int client_fd) :
     _fd(client_fd), // Corrected
     _cgiPid(0), // Corrected
    _cgiPipeFd(-1), // Corrected
    _cgiLocation(NULL) // Corrected
 {
     _parser = new HttpRequestParser(); // Initialize _parser
 }

ClientConnection::~ClientConnection() {
    delete _parser; // Delete _parser
}

int ClientConnection::getFd() const {
    return _fd; // Corrected
}

// Lê dados do socket e anexa ao buffer interno
ssize_t ClientConnection::readRequest() {
    char buffer[4096];
    ssize_t bytes_read = read(_fd, buffer, sizeof(buffer)); // Corrected

    if (bytes_read <= 0) { // Error or connection closed
        return bytes_read;
    }

    std::string data_chunk(buffer, bytes_read);
    _requestBuffer.append(data_chunk); // Append to total request buffer
    _parser->parse(data_chunk); // Feed data to the parser

    return bytes_read;
}
// Retorna o buffer com os dados da requisição
const HttpRequest& ClientConnection::getRequest() const {
    return _parser->getRequest();
}

void ClientConnection::replaceParser() {
    delete _parser;
    _parser = new HttpRequestParser();
}

size_t ClientConnection::getRequestBufferSize() const {
    if (_parser) {
        return _parser->getRequest().getBody().length();
    }
    return 0; // Or throw an error, depending on desired behavior
}

size_t ClientConnection::getTotalRequestSize() const {
    // Return the total size of the request buffer received (headers + body)
    return _requestBuffer.length();
}

bool ClientConnection::isRequestComplete() const {
    return _parser->getState() == HttpRequestParser::PARSING_COMPLETE;
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
    _cgiPid = pid; // Corrected
}

pid_t ClientConnection::getCgiPid() const {
    return _cgiPid; // Corrected
}

void ClientConnection::setCgiPipeFd(int fd) {
    _cgiPipeFd = fd; // Corrected
}

int ClientConnection::getCgiPipeFd() const {
    return _cgiPipeFd; // Corrected
}

void ClientConnection::setCgiLocation(const LocationConfig* loc) {
    _cgiLocation = loc; // Corrected
}

const LocationConfig* ClientConnection::getCgiLocation() const {
    return _cgiLocation; // Corrected
}
