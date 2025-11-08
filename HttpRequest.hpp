#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <vector>
#include <map>

class HttpRequest {
public:
    HttpRequest(const std::string& rawRequest);
    ~HttpRequest();

    const std::string& getMethod() const;
    const std::string& getUri() const;
    std::string getQueryString() const;
    const std::string& getVersion() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string getHeader(const std::string& name) const;
    const std::string& getBody() const;
    void setBody(const std::string& body);

    // For multipart/form-data
    struct UploadedFile {
        std::string name;
        std::string filename;
        std::string contentType;
        std::string content;
    };
    const std::map<std::string, std::string>& getFormData() const;
    const std::vector<UploadedFile>& getUploadedFiles() const;

private:
    void _parse(const std::string& rawRequest);
    void parseMultipartBody(const std::string& boundary);

    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;

    // For multipart/form-data
    std::map<std::string, std::string> _formData;
    std::vector<UploadedFile> _uploadedFiles;
};

#endif // HTTPREQUEST_HPP
