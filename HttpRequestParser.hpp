#ifndef HTTPREQUESTPARSER_HPP
#define HTTPREQUESTPARSER_HPP

#include <string>
#include <vector>
#include <map>
#include "HttpRequest.hpp" // The HttpRequest object it will build

class HttpRequestParser {
public:
    enum ParsingState {
        PARSING_REQUEST_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
        PARSING_CHUNKED_BODY,
        PARSING_MULTIPART_BODY, // New state for multipart/form-data
        PARSING_COMPLETE,
        PARSING_ERROR
    };

    HttpRequestParser();
    ~HttpRequestParser();

    ParsingState parse(std::string& buffer); // Feed new data to the parser
    const HttpRequest& getRequest() const; // Get the built HttpRequest object
    ParsingState getState() const; // Get current parsing state

private:
    void parseRequestLine(const std::string& line);
    void parseHeader(const std::string& line);
    void parseBody(std::string& data); // Will operate on passed data
    void parseChunkedBody(std::string& data); // Will operate on passed data
    void parseMultipartBody(std::string& data); // New method for multipart/form-data

    HttpRequest _request;
    ParsingState _state;
    // No internal buffer, operate directly on the passed string reference
    size_t _contentLength; // For non-chunked body
    size_t _bodyBytesRead; // For non-chunked body

    // For chunked transfer-encoding
    enum ChunkState {
        CHUNK_SIZE,
        CHUNK_DATA,
        CHUNK_END_CRLF, // After chunk data, expecting \r\n
        CHUNK_TRAILER_CRLF, // After 0\r\n, expecting final \r\n
        CHUNK_COMPLETE
    } _chunkState;
    size_t _currentChunkSize;
    size_t _bytesReadInChunk;
    // No _chunkBuffer here, parseChunkedBody will operate directly on _buffer

    // For multipart/form-data parsing
    std::string _multipartBoundary;
    std::string _currentPartHeaders;
    std::string _currentPartBody;
    bool _isParsingFile;
    std::string _currentFileName;
    std::string _currentFieldName;
    std::string _multipartBuffer;
    enum MultipartState {
        MULTIPART_START,
        MULTIPART_HEADERS,
        MULTIPART_BODY,
        MULTIPART_END
    } _multipartState;
}; // MISSING CLOSING BRACE FOR CLASS


#endif // HTTPREQUESTPARSER_HPP
