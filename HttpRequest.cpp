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
    if (std::getline(ss, line)) {
        std::stringstream requestLineStream(line);
        requestLineStream >> _method >> _uri >> _version;
    }

    // In a full implementation, you would loop through std::getline
    // to parse headers here.
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

std::string HttpRequest::getQueryString() const {
    size_t pos = _uri.find('?');
    if (pos != std::string::npos) {
        return _uri.substr(pos + 1);
    }
    return "";
}
