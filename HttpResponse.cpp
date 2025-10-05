#include "HttpResponse.hpp"
#include <sstream>

HttpResponse::HttpResponse() : _statusCode(0) {}

HttpResponse::~HttpResponse() {}

void HttpResponse::setStatusCode(int code, const std::string& message) {
    _statusCode = code;
    _statusMessage = message;
}

void HttpResponse::addHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void HttpResponse::setBody(const std::string& body) {
    _body = body;
}

std::string HttpResponse::toString() const {
    std::stringstream ss;

    // Status line
    ss << "HTTP/1.1 " << _statusCode << " " << _statusMessage << "\r\n";

    // Headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin(); it != _headers.end(); ++it) {
        ss << it->first << ": " << it->second << "\r\n";
    }

    // Blank line before body
    ss << "\r\n";

    // Body
    ss << _body;

    return ss.str();
}

