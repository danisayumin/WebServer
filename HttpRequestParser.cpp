#include "HttpRequestParser.hpp"
#include <sstream>
#include <iostream> // For debug
#include <cstdlib> // For strtol
#include <cstdio> // For strerr

HttpRequestParser::HttpRequestParser() :
    _state(PARSING_REQUEST_LINE),
    _contentLength(0),
    _bodyBytesRead(0),
    _chunkState(CHUNK_SIZE),
    _currentChunkSize(0),
    _bytesReadInChunk(0),
    _isParsingFile(false), // Initialize new members
    _multipartState(MULTIPART_START) // Initialize new members
{
    _multipartBuffer.clear();
}

HttpRequestParser::~HttpRequestParser() {}

HttpRequestParser::ParsingState HttpRequestParser::parse(std::string& data) {
    while (_state != PARSING_COMPLETE && _state != PARSING_ERROR) {
        if (_state == PARSING_REQUEST_LINE) {
            // Find both \r\n and \n to handle different line endings
            size_t crlf_pos = data.find("\r\n");
            size_t lf_pos = data.find("\n");
            
            size_t line_end = std::string::npos;
            size_t line_ending_len = 0;
            
            // Determine which comes first
            if (crlf_pos != std::string::npos && lf_pos != std::string::npos) {
                if (crlf_pos < lf_pos) {
                    line_end = crlf_pos;
                    line_ending_len = 2;
                } else {
                    line_end = lf_pos;
                    line_ending_len = 1;
                }
            } else if (crlf_pos != std::string::npos) {
                line_end = crlf_pos;
                line_ending_len = 2;
            } else if (lf_pos != std::string::npos) {
                line_end = lf_pos;
                line_ending_len = 1;
            }
            
            if (line_end == std::string::npos) {
                break; // Not enough data for request line
            }
            parseRequestLine(data.substr(0, line_end));
            data.erase(0, line_end + line_ending_len);
            _state = PARSING_HEADERS;
        } else if (_state == PARSING_HEADERS) {
            // Find both \r\n and \n to handle different line endings
            size_t crlf_pos = data.find("\r\n");
            size_t lf_pos = data.find("\n");
            
            size_t line_end = std::string::npos;
            size_t line_ending_len = 0;
            
            // Determine which comes first
            if (crlf_pos != std::string::npos && lf_pos != std::string::npos) {
                if (crlf_pos < lf_pos) {
                    line_end = crlf_pos;
                    line_ending_len = 2;
                } else {
                    line_end = lf_pos;
                    line_ending_len = 1;
                }
            } else if (crlf_pos != std::string::npos) {
                line_end = crlf_pos;
                line_ending_len = 2;
            } else if (lf_pos != std::string::npos) {
                line_end = lf_pos;
                line_ending_len = 1;
            }
            
            if (line_end == std::string::npos) {
                break; // Not enough data for a header line
            }
            std::string line = data.substr(0, line_end);
            data.erase(0, line_end + line_ending_len);

            if (line.empty()) { // End of headers
                std::string content_type = _request.getHeader("Content-Type");
                if (content_type.find("multipart/form-data") != std::string::npos) {
                    size_t boundary_pos = content_type.find("boundary=");
                    if (boundary_pos != std::string::npos) {
                        _multipartBoundary = "--" + content_type.substr(boundary_pos + 9);
                        std::cerr << "DEBUG: Extracted boundary: '" << _multipartBoundary << "', length: " << _multipartBoundary.length() << std::endl;
                        _state = PARSING_MULTIPART_BODY;
                        _multipartState = MULTIPART_START;
                    } else {
                        _state = PARSING_ERROR; // Malformed multipart header
                    }
                } else {
                    // Check for Transfer-Encoding: chunked
                    std::string transfer_encoding = _request.getHeader("Transfer-Encoding");
                    if (transfer_encoding == "chunked") {
                        _state = PARSING_CHUNKED_BODY;
                    } else {
                        // Get Content-Length for non-chunked body
                        std::string cl_str = _request.getHeader("Content-Length");
                        if (!cl_str.empty()) {
                            _contentLength = std::strtol(cl_str.c_str(), NULL, 10);
                        } else {
                            _contentLength = 0;
                        }
                        _state = PARSING_BODY;
                    }
                }
            } else {
                parseHeader(line);
            }
        } else if (_state == PARSING_BODY) {
            parseBody(data); // Pass data to parseBody
            if (_bodyBytesRead >= _contentLength) {
                _state = PARSING_COMPLETE;
            } else {
                break; // Not enough body data yet
            }
        } else if (_state == PARSING_CHUNKED_BODY) {
            parseChunkedBody(data); // Pass data to parseChunkedBody
            if (_chunkState == CHUNK_COMPLETE) {
                _state = PARSING_COMPLETE;
            } else {
                break; // Not enough chunked data yet
            }
        } else if (_state == PARSING_MULTIPART_BODY) {
            parseMultipartBody(data);
            if (_multipartState == MULTIPART_END) { // Assuming MULTIPART_END means complete
                _state = PARSING_COMPLETE;
            } else {
                break; // Not enough multipart data yet
            }
        }
    }
    return _state;
}

const HttpRequest& HttpRequestParser::getRequest() const {
    return _request;
}

HttpRequestParser::ParsingState HttpRequestParser::getState() const {
    return _state;
}

void HttpRequestParser::parseRequestLine(const std::string& line) {
    std::stringstream ss(line);
    std::string method, uri, version;
    ss >> method >> uri >> version;
    _request.setMethod(method); // Need setters in HttpRequest
    _request.setUri(uri);       // Need setters in HttpRequest
    _request.setVersion(version); // Need setters in HttpRequest
}

void HttpRequestParser::parseHeader(const std::string& line) {
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
        _request.addHeader(key, value); // Need addHeader in HttpRequest
    }
}

void HttpRequestParser::parseBody(std::string& data) {
    size_t to_read = std::min(_contentLength - _bodyBytesRead, data.length());
    _request.appendBody(data.substr(0, to_read));
    data.erase(0, to_read);
    _bodyBytesRead += to_read;
}

void HttpRequestParser::parseChunkedBody(std::string& data) {
    while (_chunkState != CHUNK_COMPLETE && _state != PARSING_ERROR) {
        if (_chunkState == CHUNK_SIZE) {
            // Find both \r\n and \n to handle different line endings
            size_t crlf_pos = data.find("\r\n");
            size_t lf_pos = data.find("\n");
            
            size_t line_end = std::string::npos;
            size_t line_ending_len = 0;
            
            // Determine which comes first
            if (crlf_pos != std::string::npos && lf_pos != std::string::npos) {
                if (crlf_pos < lf_pos) {
                    line_end = crlf_pos;
                    line_ending_len = 2;
                } else {
                    line_end = lf_pos;
                    line_ending_len = 1;
                }
            } else if (crlf_pos != std::string::npos) {
                line_end = crlf_pos;
                line_ending_len = 2;
            } else if (lf_pos != std::string::npos) {
                line_end = lf_pos;
                line_ending_len = 1;
            }
            
            if (line_end == std::string::npos) {
                break; // Not enough data for chunk size line
            }
            std::string size_str = data.substr(0, line_end);
            
            // Error checking for strtol
            char *endptr;
            _currentChunkSize = std::strtol(size_str.c_str(), &endptr, 16);
            if (endptr == size_str.c_str() || (*endptr != '\0' && *endptr != ';')) { // Invalid hex char or extra chars before semicolon
                _state = PARSING_ERROR; // Malformed chunk size
                return;
            }

            data.erase(0, line_end + line_ending_len); // Consume chunk size line

            if (_currentChunkSize == 0) {
                _chunkState = CHUNK_TRAILER_CRLF; // Expecting 0\r\n then optional trailers
            } else {
                _chunkState = CHUNK_DATA;
                _bytesReadInChunk = 0;
            }
        } else if (_chunkState == CHUNK_DATA) {
            size_t remaining_in_chunk = _currentChunkSize - _bytesReadInChunk;
            size_t to_read = std::min(remaining_in_chunk, data.length());

            _request.appendBody(data.substr(0, to_read));
            data.erase(0, to_read);
            _bytesReadInChunk += to_read;

            if (_bytesReadInChunk == _currentChunkSize) {
                _chunkState = CHUNK_END_CRLF; // Expecting \r\n after chunk data
            } else {
                break; // Not enough chunk data yet
            }
        } else if (_chunkState == CHUNK_END_CRLF) {
            // Accept both \r\n and \n as chunk terminator
            if (data.length() >= 2 && data.substr(0, 2) == "\r\n") {
                data.erase(0, 2); // Consume CRLF
                _chunkState = CHUNK_SIZE; // Ready for next chunk size
            } else if (data.length() >= 1 && data[0] == '\n') {
                data.erase(0, 1); // Consume LF
                _chunkState = CHUNK_SIZE; // Ready for next chunk size
            } else {
                break; // Not enough data for CRLF or malformed
            }
        } else if (_chunkState == CHUNK_TRAILER_CRLF) {
            // After 0\r\n, we expect optional trailing headers followed by a final \r\n
            // Find both \r\n and \n to handle different line endings
            size_t crlf_pos = data.find("\r\n");
            size_t lf_pos = data.find("\n");
            
            size_t line_end = std::string::npos;
            size_t line_ending_len = 0;
            
            // Determine which comes first
            if (crlf_pos != std::string::npos && lf_pos != std::string::npos) {
                if (crlf_pos < lf_pos) {
                    line_end = crlf_pos;
                    line_ending_len = 2;
                } else {
                    line_end = lf_pos;
                    line_ending_len = 1;
                }
            } else if (crlf_pos != std::string::npos) {
                line_end = crlf_pos;
                line_ending_len = 2;
            } else if (lf_pos != std::string::npos) {
                line_end = lf_pos;
                line_ending_len = 1;
            }
            
            if (line_end == std::string::npos) {
                break; // Not enough data for trailer line or final CRLF
            }
            std::string line = data.substr(0, line_end);
            data.erase(0, line_end + line_ending_len);

            if (line.empty()) { // Final \r\n, end of chunked body
                _chunkState = CHUNK_COMPLETE;
            } else { // Trailing header
                parseHeader(line); // Reuse parseHeader for trailing headers
                // Stay in CHUNK_TRAILER_CRLF to process more trailers or final CRLF
            }
        }
    }
}

void HttpRequestParser::parseMultipartBody(std::string& data) {
    _multipartBuffer.append(data);
    data.clear();

    std::cerr << "DEBUG: Entering parseMultipartBody. Multipart buffer length: " << _multipartBuffer.length() << std::endl; fflush(stderr);
    std::cerr << "DEBUG: Multipart boundary: '" << _multipartBoundary << "'" << std::endl; fflush(stderr);
    std::cerr << "DEBUG: Current multipart state: " << _multipartState << std::endl; fflush(stderr);

    while (_multipartState != MULTIPART_END && _state != PARSING_ERROR) {
        if (_multipartState == MULTIPART_START) {
            size_t boundary_pos = _multipartBuffer.find(_multipartBoundary);
            if (boundary_pos == 0) {
                _multipartBuffer.erase(0, _multipartBoundary.length());
                if (_multipartBuffer.length() >= 2 && _multipartBuffer.substr(0, 2) == "\r\n") {
                    _multipartBuffer.erase(0, 2);
                }
                _multipartState = MULTIPART_HEADERS;
                std::cerr << "DEBUG: Found initial boundary. Transitioning to MULTIPART_HEADERS." << std::endl; fflush(stderr);
            } else if (boundary_pos == std::string::npos) {
                std::cerr << "DEBUG: Initial boundary not found in buffer. Waiting for more data." << std::endl; fflush(stderr);
                break;
            } else {
                _state = PARSING_ERROR;
                std::cerr << "DEBUG: Initial boundary found but not at start. Setting PARSING_ERROR." << std::endl; fflush(stderr);
                return;
            }
        } else if (_multipartState == MULTIPART_HEADERS) {
            // Find both \r\n\r\n and \n\n to handle different line endings
            size_t crlf_pos = _multipartBuffer.find("\r\n\r\n");
            size_t lf_pos = _multipartBuffer.find("\n\n");
            
            size_t header_end = std::string::npos;
            size_t separator_len = 0;
            
            // Determine which comes first
            if (crlf_pos != std::string::npos && lf_pos != std::string::npos) {
                if (crlf_pos < lf_pos) {
                    header_end = crlf_pos;
                    separator_len = 4;
                } else {
                    header_end = lf_pos;
                    separator_len = 2;
                }
            } else if (crlf_pos != std::string::npos) {
                header_end = crlf_pos;
                separator_len = 4;
            } else if (lf_pos != std::string::npos) {
                header_end = lf_pos;
                separator_len = 2;
            }
            
            if (header_end == std::string::npos) {
                std::cerr << "DEBUG: Not enough data for multipart headers. Breaking." << std::endl; fflush(stderr);
                break;
            }
            std::string headers_str = _multipartBuffer.substr(0, header_end);
            _multipartBuffer.erase(0, header_end + separator_len);

            _currentPartHeaders = headers_str;
            std::cerr << "DEBUG: Parsed multipart headers: \n" << _currentPartHeaders << std::endl; fflush(stderr);

            std::stringstream ss_headers(headers_str);
            std::string line;
            _isParsingFile = false;
            _currentFileName = "";
            _currentFieldName = "";

            while (std::getline(ss_headers, line)) {
                if (!line.empty() && line[line.length() - 1] == '\r') {
                    line.erase(line.length() - 1);
                }
                if (line.empty()) {
                    continue;
                }
                
                size_t colon_pos = line.find(":");
                if (colon_pos != std::string::npos) {
                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);
                    size_t first_char = value.find_first_not_of(" \t");
                    if (first_char != std::string::npos) {
                        value = value.substr(first_char);
                    }

                    if (key == "Content-Disposition") {
                        size_t filename_pos = value.find("filename=\"");
                        if (filename_pos != std::string::npos) {
                            _isParsingFile = true;
                            size_t filename_start = filename_pos + 10;
                            size_t filename_end = value.find("\"", filename_start);
                            if (filename_end != std::string::npos) {
                                _currentFileName = value.substr(filename_start, filename_end - filename_start);
                            }
                        }
                        size_t name_pos = value.find("name=\"");
                        if (name_pos != std::string::npos) {
                            size_t name_start = name_pos + 6;
                            size_t name_end = value.find("\"", name_start);
                            if (name_end != std::string::npos) {
                                _currentFieldName = value.substr(name_start, name_end - name_start);
                            }
                        }
                    }
                }
            }
            std::cerr << "DEBUG: isParsingFile: " << _isParsingFile << ", FileName: '" << _currentFileName << "', FieldName: '" << _currentFieldName << "'" << std::endl; fflush(stderr);
            _multipartState = MULTIPART_BODY;
        } else if (_multipartState == MULTIPART_BODY) {
            size_t boundary_pos = _multipartBuffer.find(_multipartBoundary);

            if (boundary_pos == std::string::npos) {
                std::cerr << "DEBUG: In MULTIPART_BODY, boundary length is: " << _multipartBoundary.length() << std::endl;
                // Boundary not found. To avoid consuming a partial boundary at the end,
                // we only process a "safe" part of the buffer, leaving a tail that
                // might contain the start of a boundary.
                size_t safe_consume_len = 0;
                if (_multipartBuffer.length() > _multipartBoundary.length()) {
                    safe_consume_len = _multipartBuffer.length() - _multipartBoundary.length();
                }

                if (safe_consume_len > 0) {
                    _currentPartBody.append(_multipartBuffer.substr(0, safe_consume_len));
                    _multipartBuffer.erase(0, safe_consume_len);
                    std::cerr << "DEBUG: No boundary found, consuming " << safe_consume_len << " bytes. Current part body length: " << _currentPartBody.length() << std::endl; fflush(stderr);
                } else {
                    std::cerr << "DEBUG: No boundary found, buffer too small to consume safely. Waiting for more data." << std::endl; fflush(stderr);
                }
                break; // We need more data to find the boundary.
            }

            // Boundary was found. The data before it is the end of the current part's body.
            std::string part_data = _multipartBuffer.substr(0, boundary_pos);
            if (part_data.length() >= 2 && part_data.substr(part_data.length() - 2) == "\r\n") {
                part_data.erase(part_data.length() - 2);
            }
            _currentPartBody.append(part_data);
            std::cerr << "DEBUG: Found boundary at " << boundary_pos << ". Final part body length: " << _currentPartBody.length() << std::endl; fflush(stderr);

            // Store the completed part
            if (_isParsingFile) {
                _request.addUploadedFile(_currentFieldName, _currentFileName, _currentPartBody);
                std::cerr << "DEBUG: Added uploaded file: " << _currentFileName << " (Field: " << _currentFieldName << ")" << std::endl; fflush(stderr);
            } else {
                _request.addFormField(_currentFieldName, _currentPartBody);
                std::cerr << "DEBUG: Added form field: " << _currentFieldName << " = '" << _currentPartBody << "'" << std::endl; fflush(stderr);
            }

            // Reset for the next part
            _currentPartBody.clear();
            _isParsingFile = false;
            _currentFileName.clear();
            _currentFieldName.clear();

            // Consume the boundary from the buffer
            _multipartBuffer.erase(0, boundary_pos);

            // Check if it's an end boundary ("--") and consume it
            if (_multipartBuffer.length() >= _multipartBoundary.length() + 2 && _multipartBuffer.substr(_multipartBoundary.length(), 2) == "--") {
                _multipartBuffer.erase(0, _multipartBoundary.length() + 2);
                 // Consume trailing CRLF
                if (_multipartBuffer.length() >= 2 && _multipartBuffer.substr(0, 2) == "\r\n") {
                    _multipartBuffer.erase(0, 2);
                }
                _multipartState = MULTIPART_END;
                std::cerr << "DEBUG: Found end boundary. Transitioning to MULTIPART_END." << std::endl; fflush(stderr);
            } else {
                // It's a regular boundary, consume it and the trailing CRLF
                _multipartBuffer.erase(0, _multipartBoundary.length());
                if (_multipartBuffer.length() >= 2 && _multipartBuffer.substr(0, 2) == "\r\n") {
                    _multipartBuffer.erase(0, 2);
                }
                _multipartState = MULTIPART_HEADERS;
                std::cerr << "DEBUG: Found part boundary. Transitioning to MULTIPART_HEADERS for next part." << std::endl; fflush(stderr);
            }
        }
    }
}
