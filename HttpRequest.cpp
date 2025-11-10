#include "HttpRequest.hpp"
#include <sstream>
#include <vector>
#include <iostream> // Added for debug output

HttpRequest::HttpRequest() :
    _method(""),
    _uri(""),
    _version(""),
    _body(""),
    _queryString("") // Initialize _queryString
{
    // _headers, _formFields, _uploadedFiles are default-constructed empty
}

HttpRequest::~HttpRequest() {}

void HttpRequest::setMethod(const std::string& method) { _method = method; }
void HttpRequest::setUri(const std::string& uri) {
    size_t query_pos = uri.find('?');
    if (query_pos != std::string::npos) {
        _uri = uri.substr(0, query_pos);
        _queryString = uri.substr(query_pos + 1);
    } else {
        _uri = uri;
        _queryString = "";
    }
}
void HttpRequest::setVersion(const std::string& version) { _version = version; }
void HttpRequest::addHeader(const std::string& name, const std::string& value) { _headers[name] = value; }
void HttpRequest::setBody(const std::string& body) { _body = body; }
void HttpRequest::appendBody(const std::string& data) { _body.append(data); }

const std::string& HttpRequest::getMethod() const { return _method; }
const std::string& HttpRequest::getUri() const { return _uri; }
const std::string& HttpRequest::getVersion() const { return _version; }
const std::string HttpRequest::getQueryString() const { return _queryString; } // Return by value as it's a copy
const std::map<std::string, std::string>& HttpRequest::getHeaders() const { return _headers; }

const std::string& HttpRequest::getHeader(const std::string& name) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(name);
    if (it != _headers.end()) {
        return it->second;
    }
    // Return a reference to an empty string if header not found
    static const std::string empty_string = "";
    return empty_string;
}

const std::string& HttpRequest::getBody() const { return _body; }

void HttpRequest::addUploadedFile(const std::string& fieldName, const std::string& filename, const std::string& content) {
    UploadedFile file;
    file.fieldName = fieldName;
    file.filename = filename;
    file.content = content;
    _uploadedFiles.push_back(file);
}

void HttpRequest::addFormField(const std::string& fieldName, const std::string& value) {
    _formFields[fieldName] = value;
}

const std::vector<HttpRequest::UploadedFile>& HttpRequest::getUploadedFiles() const {
    return _uploadedFiles;
}

const std::map<std::string, std::string>& HttpRequest::getFormFields() const {
    return _formFields;
}