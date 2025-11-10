#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <vector>
#include <map>

class HttpRequest {
public:
    HttpRequest(); // Default constructor
    ~HttpRequest();

    const std::string& getMethod() const;
    void setMethod(const std::string& method); // Added
    const std::string& getUri() const;
    void setUri(const std::string& uri); // Added
    const std::string getQueryString() const;
    const std::string& getVersion() const;
    void setVersion(const std::string& version); // Added
    const std::map<std::string, std::string>& getHeaders() const;
    void addHeader(const std::string& name, const std::string& value); // Added
    struct UploadedFile {
        std::string fieldName;
        std::string filename;
        std::string content;
    };

    const std::string& getHeader(const std::string& name) const; // Corrected return type
    const std::string& getBody() const;
    void setBody(const std::string& body); // Added
    void appendBody(const std::string& data);

    // New methods for multipart parsing
    void addUploadedFile(const std::string& fieldName, const std::string& filename, const std::string& content);
    void addFormField(const std::string& fieldName, const std::string& value);
    const std::vector<UploadedFile>& getUploadedFiles() const;
    const std::map<std::string, std::string>& getFormFields() const;


private:
    std::string _method;
    std::string _uri;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::string _queryString;

    // New members for multipart parsing
    std::vector<UploadedFile> _uploadedFiles;
    std::map<std::string, std::string> _formFields;
};

#endif // HTTPREQUEST_HPP
