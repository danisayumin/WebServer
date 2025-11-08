#include "HttpRequest.hpp"
#include <sstream>
#include <vector>
#include <iostream> // Added for debug output

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

    // If multipart/form-data, parse it
    std::string content_type = getHeader("Content-Type");
    if (content_type.find("multipart/form-data") != std::string::npos) {
        size_t boundary_pos = content_type.find("boundary=");
        if (boundary_pos != std::string::npos) {
            std::string boundary = content_type.substr(boundary_pos + 9);
            parseMultipartBody(boundary);
        }
    }
}

void HttpRequest::parseMultipartBody(const std::string& boundary) {
    std::string delimiter = "--" + boundary;

    size_t pos = 0;
    while (true) {
        size_t start_of_part = _body.find(delimiter, pos);
        if (start_of_part == std::string::npos) {
            break; // No more boundaries
        }

        // Move past the boundary delimiter itself
        start_of_part += delimiter.length();

        // Check for the final boundary "--"
        if (_body.compare(start_of_part, 2, "--") == 0) {
            break; // End of multipart data
        }

        // Skip CRLF after boundary
        if (_body.compare(start_of_part, 2, "\r\n") == 0) {
            start_of_part += 2;
        }

        // Find the start of the next boundary, which is the end of our current part
        size_t end_of_part = _body.find(delimiter, start_of_part);
        if (end_of_part == std::string::npos) {
            break; // Malformed, no closing boundary
        }

        std::string part = _body.substr(start_of_part, end_of_part - start_of_part);

        // Find headers end
        size_t headers_end = part.find("\r\n\r\n");
        if (headers_end == std::string::npos) {
            pos = end_of_part; // Malformed part, skip
            continue;
        }

        std::string part_headers_str = part.substr(0, headers_end);
        std::string part_body = part.substr(headers_end + 4);

        // Trim trailing CRLF from part_body
        if (part_body.length() >= 2 && part_body.substr(part_body.length() - 2) == "\r\n") {
            part_body.erase(part_body.length() - 2);
        }

        std::map<std::string, std::string> part_headers;
        std::stringstream ss_part_headers(part_headers_str);
        std::string line;
        while (std::getline(ss_part_headers, line) && !line.empty() && line != "\r") {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                size_t first_char = value.find_first_not_of(" \t");
                if (first_char != std::string::npos) {
                    value = value.substr(first_char);
                }
                size_t last_char = value.find_last_not_of(" \t\r\n");
                if (last_char != std::string::npos) {
                    value = value.substr(0, last_char + 1);
                }
                part_headers[key] = value;
            }
        }

        std::string content_disposition = part_headers["Content-Disposition"];
        if (!content_disposition.empty()) {
            size_t name_pos = content_disposition.find("name=\"");
            std::string name;
            if (name_pos != std::string::npos) {
                name_pos += 6;
                size_t name_end_pos = content_disposition.find("\"", name_pos);
                if (name_end_pos != std::string::npos) {
                    name = content_disposition.substr(name_pos, name_end_pos - name_pos);
                }
            }

            size_t filename_pos = content_disposition.find("filename=\"");
            if (filename_pos != std::string::npos) { // This is a file
                filename_pos += 10;
                size_t filename_end_pos = content_disposition.find("\"", filename_pos);
                std::string filename;
                if (filename_end_pos != std::string::npos) {
                    filename = content_disposition.substr(filename_pos, filename_end_pos - filename_pos);
                }

                UploadedFile file;
                file.name = name;
                file.filename = filename;
                file.contentType = part_headers["Content-Type"];
                file.content = part_body;
                _uploadedFiles.push_back(file);
            } else { // Regular form field
                _formData[name] = part_body;
            }
        }
        
        // Set position for next search
        pos = end_of_part;
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

const std::map<std::string, std::string>& HttpRequest::getFormData() const {
    return _formData;
}

const std::vector<HttpRequest::UploadedFile>& HttpRequest::getUploadedFiles() const {
    return _uploadedFiles;
}

std::string HttpRequest::getQueryString() const {

    size_t pos = _uri.find('?');
    if (pos != std::string::npos) {
        return _uri.substr(pos + 1);
    }
    return "";
}
