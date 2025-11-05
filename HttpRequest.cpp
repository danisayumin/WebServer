#include "HttpRequest.hpp"
#include <sstream>
#include <vector>

HttpRequest::HttpRequest(const std::string& rawRequest) {
    _parse(rawRequest);
}

HttpRequest::~HttpRequest() {}

void HttpRequest::_parse(const std::string& rawRequest) {
    std::stringstream ss(rawRequest);
    std::string line;

    // Parse request line
    if (std::getline(ss, line) && !line.empty()) {
        std::stringstream requestLineStream(line);
        requestLineStream >> _method >> _uri >> _version;
    }

    size_t headers_end_pos = rawRequest.find("\r\n\r\n");
    if (headers_end_pos == std::string::npos) {
        // Handle error: malformed request, no end of headers
        // For now, we'll just assume no body if headers end not found
        return;
    }

    std::string headers_part = rawRequest.substr(0, headers_end_pos);
    std::stringstream ss_headers(headers_part);

    // Parse headers
    while (std::getline(ss_headers, line) && !line.empty() && line != "\r") {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim leading whitespace from value
            size_t first_char = value.find_first_not_of(" \t");
            if (first_char != std::string::npos) {
                value = value.substr(first_char);
            }
            
            // Trim trailing whitespace (especially \r)
            size_t last_char = value.find_last_not_of(" \t\r\n");
            if (last_char != std::string::npos) {
                value = value.substr(0, last_char + 1);
            }

            _headers[key] = value;
        }
    }

    // Extract body
    if (rawRequest.length() > headers_end_pos + 4) {
        _body = rawRequest.substr(headers_end_pos + 4);
    }
}

const std::string& HttpRequest::getMethod() const {
    return _method;
}

const std::string& HttpRequest::getUri() const {
    return _uri;
}

const std::string& HttpRequest::getVersion() const {
    return _version;
}

const std::map<std::string, std::string>& HttpRequest::getHeaders() const {
    return _headers;
}

const std::string HttpRequest::getHeader(const std::string& name) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(name);
    if (it != _headers.end()) {
        return it->second;
    }
    return "";
}

const std::string& HttpRequest::getBody() const {
    return _body;
}

void HttpRequest::setBody(const std::string& body) {
    _body = body;
}

std::string HttpRequest::getQueryString() const {
    size_t pos = _uri.find('?');
    if (pos != std::string::npos) {
        return _uri.substr(pos + 1);
    }
    return "";
}
