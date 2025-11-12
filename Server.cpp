#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include <stdexcept>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cerrno>
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio> // For std::remove
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <signal.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctime> // For time tracking

std::string getMimeType(const std::string& filePath) {
    static std::map<std::string, std::string> mimeTypes;
    if (mimeTypes.empty()) {
        mimeTypes[".html"] = "text/html";
        mimeTypes[".htm"] = "text/html";
        mimeTypes[".css"] = "text/css";
        mimeTypes[".js"] = "application/javascript";
        mimeTypes[".jpg"] = "image/jpeg";
        mimeTypes[".jpeg"] = "image/jpeg";
        mimeTypes[".png"] = "image/png";
        mimeTypes[".gif"] = "image/gif";
        mimeTypes[".svg"] = "image/svg+xml";
        mimeTypes[".ico"] = "image/x-icon";
        mimeTypes[".txt"] = "text/plain";
    }

    size_t dot_pos = filePath.rfind('.');
    if (dot_pos == std::string::npos) {
        return "application/octet-stream";
    }

    std::string ext = filePath.substr(dot_pos);
    std::map<std::string, std::string>::const_iterator it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }

    return "application/octet-stream";
}

Server::Server(const ConfigParser& config) : _config(config), _max_fd(0) {
    FD_ZERO(&_master_set);
    FD_ZERO(&_write_fds);

    const std::vector<int>& ports = _config.getPorts();
    if (ports.empty()) {
        throw std::runtime_error("No ports specified in configuration.");
    }

    for (size_t i = 0; i < ports.size(); ++i) {
        int fd = _setupServerSocket(ports[i]);
        _listen_fds.push_back(fd);
        FD_SET(fd, &_master_set);
        if (fd > _max_fd) _max_fd = fd;
    }
}

Server::~Server() {
    for (size_t i = 0; i < _listen_fds.size(); ++i) {
        if (_listen_fds[i] > 0) close(_listen_fds[i]);
    }
    for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
        close(it->first);
        delete it->second;
    }
}

int Server::_setupServerSocket(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) throw std::runtime_error("socket() failed");
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        close(fd);
        throw std::runtime_error("setsockopt() failed");
    }
    if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        close(fd);
        throw std::runtime_error("fcntl() failed");
    }
    sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        close(fd);
        throw std::runtime_error("bind() failed");
    }
    if (listen(fd, SOMAXCONN) < 0) {
        close(fd);
        throw std::runtime_error("listen() failed");
    }
    std::cout << "Server listening on port " << port << std::endl;
    return fd;
}

void Server::run() {
    std::cout << "Server ready. Waiting for connections..." << std::endl;
    while (true) {
        fd_set read_fds = _master_set;
        fd_set write_fds = _write_fds;

        if (select(_max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0) {
            perror("select");
            continue;
        }

        // Check for CGI timeouts
        _checkCgiTimeouts();

        for (int fd = 0; fd <= _max_fd; ++fd) {
            if (FD_ISSET(fd, &read_fds)) {
                bool is_listening_fd = false;
                for (size_t i = 0; i < _listen_fds.size(); ++i) {
                    if (fd == _listen_fds[i]) {
                        _acceptNewConnection(fd);
                        is_listening_fd = true;
                        break;
                    }
                }
                if (is_listening_fd) continue;
                
                if (_pipe_to_client_map.count(fd)) {
                    _handleCgiRead(fd);
                } else if (_clients.count(fd)) {
                    _handleClientData(fd);
                }
            }
            if (FD_ISSET(fd, &write_fds)) {
                if (_cgi_stdin_pipe_to_client_map.count(fd)) {
                    _handleCgiWrite(fd);
                } else {
                    _handleClientWrite(fd);
                }
            }
        }
    }
}

void Server::_acceptNewConnection(int listening_fd) {
    int client_fd = accept(listening_fd, NULL, NULL);
    if (client_fd < 0) return;
    
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    FD_SET(client_fd, &_master_set);
    
    if (_clients.find(client_fd) != _clients.end()) {
        delete _clients[client_fd];
    }
    _clients[client_fd] = new ClientConnection(client_fd);

    if (client_fd > _max_fd) _max_fd = client_fd;
    
    std::cout << "New connection: " << client_fd << std::endl;
}

void Server::_handleClientData(int client_fd) {
    if (_clients.find(client_fd) == _clients.end()) return;
    ClientConnection* client = _clients[client_fd];

    if (client->readRequest() > 0) {
        // Find the corresponding location to check client_max_body_size
        const HttpRequest& temp_req = client->getRequest();
        const std::vector<LocationConfig*>& locations = _config.getLocations();
        const LocationConfig* matched_location = NULL;
        std::string longest_match = "";

        for (size_t i = 0; i < locations.size(); ++i) {
            if (temp_req.getUri().rfind(locations[i]->path, 0) == 0) {
                if (locations[i]->path.length() > longest_match.length()) {
                    longest_match = locations[i]->path;
                    matched_location = locations[i];
                }
            }
        }

        size_t max_body_size = 1 * 1024 * 1024; // Default 1MB
        if (matched_location) {
            max_body_size = matched_location->client_max_body_size;
        }

        if (client->getRequestBufferSize() > max_body_size) {
            _sendErrorResponse(client, 413, "Payload Too Large", matched_location);
            client->replaceParser();
            return;
        }

        if (client->isRequestComplete()) {
            const HttpRequest& req = client->getRequest();
            HttpResponse res;

            // Enforce allowed methods if configured for the matched location
            if (matched_location && !matched_location->allowed_methods.empty()) {
                bool method_allowed = false;
                for (size_t i = 0; i < matched_location->allowed_methods.size(); ++i) {
                    if (req.getMethod() == matched_location->allowed_methods[i]) {
                        method_allowed = true;
                        break;
                    }
                }
                if (!method_allowed) {
                    _sendErrorResponse(client, 405, "Method Not Allowed", matched_location);
                    client->replaceParser();
                    return;
                }
            }

            // Handle redirection if configured
            if (matched_location && !matched_location->redirect.empty()) {
                res.setStatusCode(301, "Moved Permanently");
                res.addHeader("Location", matched_location->redirect);
                client->setResponse(res.toString());
                FD_SET(client_fd, &_write_fds);
                client->replaceParser();
                return;
            }

            bool is_cgi = false;
            if (matched_location && !matched_location->cgi_path.empty() && !matched_location->cgi_ext.empty()) {
                const std::string& ext = matched_location->cgi_ext;
                const std::string& uri = req.getUri();
                if (uri.length() >= ext.length() && uri.substr(uri.length() - ext.length()) == ext) {
                    is_cgi = true;
                }
            }

            if (is_cgi) {
                _executeCgi(client, matched_location);
                return;
            }

            // Handle /api/uploads GET endpoint - list files with modification times
            if (req.getUri() == "/api/uploads" && req.getMethod() == "GET") {
                std::string uploads_dir = "./www/uploads";
                DIR* dir = opendir(uploads_dir.c_str());
                
                if (!dir) {
                    res.setStatusCode(500, "Internal Server Error");
                    res.addHeader("Content-Type", "application/json");
                    std::string body = "{\"error\": \"Unable to open uploads directory\"}";
                    std::stringstream ss_len; ss_len << body.length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(body);
                    client->setResponse(res.toString());
                    FD_SET(client_fd, &_write_fds);
                    client->replaceParser();
                    return;
                }

                // Build JSON response with files
                std::stringstream json_body;
                json_body << "{\"files\": [";
                bool first = true;

                struct dirent* ent;
                while ((ent = readdir(dir)) != NULL) {
                    // Skip directories and hidden files
                    if (ent->d_type != DT_REG || ent->d_name[0] == '.') {
                        continue;
                    }

                    // Get file modification time
                    std::string full_path = uploads_dir + "/" + ent->d_name;
                    struct stat file_stat;
                    if (stat(full_path.c_str(), &file_stat) != 0) {
                        continue; // Skip files we can't stat
                    }

                    // Add JSON object for this file
                    if (!first) json_body << ",";
                    first = false;

                    // Convert modification time to milliseconds
                    long long modified_ms = (long long)file_stat.st_mtime * 1000;
                    json_body << "{\"name\": \"" << ent->d_name << "\", \"modified\": " << modified_ms << "}";
                }

                closedir(dir);
                json_body << "]}";

                res.setStatusCode(200, "OK");
                res.addHeader("Content-Type", "application/json");
                std::string body = json_body.str();
                std::stringstream ss_len; ss_len << body.length();
                res.addHeader("Content-Length", ss_len.str());
                res.setBody(body);
                client->setResponse(res.toString());
                FD_SET(client_fd, &_write_fds);
                client->replaceParser();
                return;
            }

            if (req.getMethod() == "DELETE") {
                // Handle DELETE /uploads/{filename}
                if (req.getUri().find("/uploads/") == 0) {
                    std::string uploads_dir = "./www/uploads";
                    // Extract filename from URI: /uploads/{filename}
                    std::string filename = req.getUri().substr(9); // strlen("/uploads/") == 9
                    
                    // Security: prevent directory traversal
                    if (filename.find("..") != std::string::npos || filename.find("/") != std::string::npos) {
                        res.setStatusCode(400, "Bad Request");
                        client->setResponse(res.toString());
                        FD_SET(client_fd, &_write_fds);
                        client->replaceParser();
                        return;
                    }

                    std::string filePath = uploads_dir + "/" + filename;

                    if (access(filePath.c_str(), F_OK) == 0) {
                        if (std::remove(filePath.c_str()) == 0) {
                            res.setStatusCode(204, "No Content");
                        } else {
                            if (errno == EACCES) {
                                res.setStatusCode(403, "Forbidden");
                            } else {
                                res.setStatusCode(500, "Internal Server Error");
                            }
                        }
                    } else {
                        res.setStatusCode(404, "Not Found");
                    }
                    client->setResponse(res.toString());
                    FD_SET(client_fd, &_write_fds);
                    client->replaceParser();
                    return;
                }

                // Original DELETE handler for other paths
                std::string root = _config.getRoot();
                if (matched_location && !matched_location->root.empty()) {
                    root = matched_location->root;
                }
                std::string filePath = root + req.getUri();

                if (access(filePath.c_str(), F_OK) == 0) {
                    if (std::remove(filePath.c_str()) == 0) {
                        res.setStatusCode(204, "No Content");
                    } else {
                        if (errno == EACCES) {
                            res.setStatusCode(403, "Forbidden");
                        } else {
                            res.setStatusCode(500, "Internal Server Error");
                        }
                    }
                } else {
                    res.setStatusCode(404, "Not Found");
                }
            } else if (req.getMethod() == "POST") {
                // Uploads are now handled by checking if a location has an upload_path
                if (matched_location && !matched_location->upload_path.empty()) {
                    std::string content_type = req.getHeader("Content-Type");
                    if (content_type.find("multipart/form-data") != std::string::npos) {
                        const std::vector<HttpRequest::UploadedFile>& uploadedFiles = req.getUploadedFiles();
                        if (uploadedFiles.empty()) {
                            res.setStatusCode(400, "Bad Request");
                            res.setBody("No files uploaded.");
                        } else {
                            bool all_saved = true;
                            std::string upload_dir = matched_location->upload_path;
                            // Ensure upload_dir ends with a slash
                            if (upload_dir[upload_dir.length() - 1] != '/') {
                                upload_dir += "/";
                            }

                            // Check if directory exists and is writable
                            struct stat dir_stat;
                            if (stat(upload_dir.c_str(), &dir_stat) != 0) {
                                all_saved = false;
                                res.setStatusCode(500, "Internal Server Error");
                                std::string body = "Upload directory does not exist or is inaccessible.";
                                res.setBody(body);
                                std::stringstream ss_len; ss_len << body.length();
                                res.addHeader("Content-Length", ss_len.str());
                                client->setResponse(res.toString());
                                FD_SET(client_fd, &_write_fds);
                                client->replaceParser();
                                return;
                            }
                            if (!S_ISDIR(dir_stat.st_mode)) {
                                all_saved = false;
                                res.setStatusCode(500, "Internal Server Error");
                                std::string body = "Upload path is not a directory.";
                                res.setBody(body);
                                std::stringstream ss_len; ss_len << body.length();
                                res.addHeader("Content-Length", ss_len.str());
                                client->setResponse(res.toString());
                                FD_SET(client_fd, &_write_fds);
                                client->replaceParser();
                                return;
                            }
                            if (access(upload_dir.c_str(), W_OK) != 0) {
                                all_saved = false;
                                res.setStatusCode(403, "Forbidden");
                                std::string body = "Upload directory is not writable.";
                                res.setBody(body);
                                std::stringstream ss_len; ss_len << body.length();
                                res.addHeader("Content-Length", ss_len.str());
                                client->setResponse(res.toString());
                                FD_SET(client_fd, &_write_fds);
                                client->replaceParser();
                                return;
                            }

                            for (size_t i = 0; i < uploadedFiles.size(); ++i) {
                                const HttpRequest::UploadedFile& file = uploadedFiles[i];
                                std::string safe_filename = upload_dir;
                                // Basic sanitization: remove path separators
                                size_t last_slash = file.filename.find_last_of("/");
                                if (last_slash == std::string::npos) {
                                    last_slash = file.filename.find_last_of("\\");
                                }
                                if (last_slash != std::string::npos) {
                                    safe_filename += file.filename.substr(last_slash + 1);
                                } else {
                                    safe_filename += file.filename;
                                }

                                std::ofstream outfile(safe_filename.c_str(), std::ios::binary);
                                if (outfile.is_open()) {
                                    outfile.write(file.content.c_str(), file.content.length());
                                    outfile.close();
                                    std::cout << "Uploaded file saved to: " << safe_filename << std::endl;
                                } else {
                                    all_saved = false;
                                    break;
                                }
                            }
                            if (all_saved) {
                                res.setStatusCode(302, "Found");
                                res.addHeader("Location", "/uploaded.html");
                                res.addHeader("Content-Length", "0");
                            } else {
                                res.setStatusCode(500, "Internal Server Error");
                                std::string body = "Failed to save some files.";
                                res.setBody(body);
                                std::stringstream ss_len; ss_len << body.length();
                                res.addHeader("Content-Length", ss_len.str());
                            }
                        }
                    } else {
                        res.setStatusCode(400, "Bad Request");
                        std::string body = "Unsupported Content-Type for upload.";
                        res.setBody(body);
                        std::stringstream ss_len; ss_len << body.length();
                        res.addHeader("Content-Length", ss_len.str());
                    }
                    client->setResponse(res.toString()); // ADDED THIS LINE
                } else { // Other POST requests - return 405
                    _sendErrorResponse(client, 405, "Method Not Allowed", matched_location);
                    client->replaceParser();
                    return;
                }
            } else { // GET method
                std::string root = _config.getRoot();
                if (matched_location && !matched_location->root.empty()) {
                    root = matched_location->root;
                }

                std::string uri = req.getUri();
                if (uri == "/") {
                    uri = "/index.html";
                    if (matched_location && !matched_location->index.empty()) {
                        uri = "/" + matched_location->index;
                    }
                }
                
                                std::string filePath = root + uri;
                                std::ifstream file(filePath.c_str());
                bool file_found = file.is_open();

                if (!file_found) {
                    // If not found, and URI doesn't have an extension, try appending .html
                    size_t dot_pos = uri.rfind('.');
                    // Check if there's no dot, or if the dot is part of a directory name (e.g., /path.to/file)
                    if (dot_pos == std::string::npos || dot_pos < uri.rfind('/')) {
                        std::string html_filePath = filePath + ".html";
                        std::ifstream html_file(html_filePath.c_str());
                        if (html_file.is_open()) {
                            filePath = html_filePath; // Update filePath to the .html version
                            file.close(); // Use the opened .html file
                            file_found = true;
                        }
                    }
                }

                if (file_found) {
                    // It's a file, serve it
                    std::stringstream buffer; buffer << file.rdbuf();
                    res.setStatusCode(200, "OK");
                    res.addHeader("Content-Type", getMimeType(filePath));
                    std::stringstream ss_len; ss_len << buffer.str().length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(buffer.str());
                    client->setResponse(res.toString());
                } else {
                    // File not found, check if it's a directory for autoindex or 404
                    struct stat path_stat;
                    if (stat(filePath.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
                        // It's a directory, check for index file or autoindex
                        if (uri[uri.length() - 1] != '/') {
                            uri += "/";
                        }
                        std::string index_file_path = filePath; // Use the original filePath (which is a directory)
                        if (index_file_path[index_file_path.length() - 1] != '/') {
                            index_file_path += "/";
                        }
                        index_file_path += "index.html"; // Default index file
                        if (matched_location && !matched_location->index.empty()) {
                            index_file_path = filePath; // Reset to directory path
                            if (index_file_path[index_file_path.length() - 1] != '/') {
                                index_file_path += "/";
                            }
                            index_file_path += matched_location->index;
                        }
                        
                        std::ifstream index_file(index_file_path.c_str());
                        if (index_file.is_open()) {
                            // Serve index file
                            std::stringstream buffer; buffer << index_file.rdbuf();
                            res.setStatusCode(200, "OK");
                            res.addHeader("Content-Type", "text/html");
                            std::stringstream ss_len; ss_len << buffer.str().length();
                            res.addHeader("Content-Length", ss_len.str());
                            res.setBody(buffer.str());
                            client->setResponse(res.toString());
                        } else {
                            // No index file, check for autoindex
                            if (matched_location && matched_location->autoindex) {
                                // Generate directory listing
                                std::stringstream body_ss;
                                body_ss << "<html><head><title>Index of " << uri << "</title></head><body><h1>Index of " << uri << "</h1><hr><ul>";
                                
                                DIR* dir = opendir(filePath.c_str());
                                if (dir) {
                                    struct dirent* ent;
                                    while ((ent = readdir(dir)) != NULL) {
                                        std::string name = ent->d_name;
                                        if (name == ".") continue;
                                        body_ss << "<li><a href=\"" << uri << (name == ".." ? "" : name) << (name == ".." ? "" : (ent->d_type == DT_DIR ? "/" : "")) << "\">" << name << (ent->d_type == DT_DIR ? "/" : "") << "</a></li>";
                                    }
                                    closedir(dir);
                                }
                                body_ss << "</ul><hr></body></html>";

                                res.setStatusCode(200, "OK");
                                res.addHeader("Content-Type", "text/html");
                                std::string body = body_ss.str();
                                std::stringstream ss_len; ss_len << body.length();
                                res.addHeader("Content-Length", ss_len.str());
                                res.setBody(body);
                            } else {
                                // No index and autoindex is off
                                _sendErrorResponse(client, 403, "Forbidden", matched_location);
                                client->replaceParser();
                                return;
                            }
                        }
                    } else {
                        // Not a file and not a directory
                        _sendErrorResponse(client, 404, "Not Found", matched_location);
                        client->replaceParser();
                        return;
                    }
                }
            }
            client->replaceParser();
            FD_SET(client_fd, &_write_fds);
        }
    } else {
        close(client_fd);
        FD_CLR(client_fd, &_master_set);
        FD_CLR(client_fd, &_write_fds);
        delete _clients[client_fd];
        _clients.erase(client_fd);
    }
}

void Server::_executeCgi(ClientConnection* client, const LocationConfig* loc) {
    int cgi_stdout_pipe[2]; // Pipe for CGI to write its stdout to
    int cgi_stdin_pipe[2];  // Pipe for server to write request body to CGI's stdin

    if (pipe(cgi_stdout_pipe) < 0 || pipe(cgi_stdin_pipe) < 0) {
        std::cerr << "CGI Error: pipe() failed" << std::endl; fflush(stderr);
        _sendErrorResponse(client, 500, "Internal Server Error: pipe() failed", loc);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "CGI Error: fork() failed" << std::endl; fflush(stderr);
        close(cgi_stdout_pipe[0]); close(cgi_stdout_pipe[1]);
        close(cgi_stdin_pipe[0]); close(cgi_stdin_pipe[1]);
        _sendErrorResponse(client, 500, "Internal Server Error: fork() failed", loc);
        return;
    }

    if (pid == 0) { // Child Process
        close(cgi_stdout_pipe[0]); // Child doesn't read from stdout pipe
        close(cgi_stdin_pipe[1]);  // Child doesn't write to stdin pipe

        // Redirect child's stdin to be the read end of the stdin pipe
        if (dup2(cgi_stdin_pipe[0], STDIN_FILENO) == -1) {
            perror("dup2 STDIN_FILENO failed"); exit(EXIT_FAILURE);
        }
        // Redirect child's stdout and stderr to be the write end of the stdout pipe
        if (dup2(cgi_stdout_pipe[1], STDOUT_FILENO) == -1) {
            perror("dup2 STDOUT_FILENO failed"); exit(EXIT_FAILURE);
        }
        if (dup2(cgi_stdout_pipe[1], STDERR_FILENO) == -1) {
            perror("dup2 STDERR_FILENO failed"); exit(EXIT_FAILURE);
        }
        close(cgi_stdout_pipe[1]);
        const HttpRequest& req = client->getRequest();

        std::string root = _config.getRoot();
        if (loc && !loc->root.empty()) {
            root = loc->root;
        }

        std::string script_name_uri = req.getUri();
        size_t query_pos = script_name_uri.find('?');
        if (query_pos != std::string::npos) {
            script_name_uri = script_name_uri.substr(0, query_pos);
        }

        // Convert relative root to absolute path
        char abs_root[1024];
        if (realpath(root.c_str(), abs_root) == NULL) {
            perror("realpath failed for root");
            exit(EXIT_FAILURE);
        }
        std::string absolute_root(abs_root);

        // Change to the location's root directory for CGI execution
        if (chdir(absolute_root.c_str()) != 0) {
            perror("chdir failed for CGI script");
            exit(EXIT_FAILURE);
        }

        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd failed after chdir");
            exit(EXIT_FAILURE);
        }

        std::string script_filename = std::string(cwd) + script_name_uri;
        // Check if the script file exists and is executable
        struct stat path_stat;
        if (stat(script_filename.c_str(), &path_stat) != 0) {
            perror("stat failed for CGI script");
            exit(EXIT_FAILURE);
        }
        if (!(path_stat.st_mode & S_IXUSR)) {
            perror("CGI script is not executable");
            exit(EXIT_FAILURE);
        }

        char** argv_c = new char*[3];
        argv_c[0] = new char[loc->cgi_path.length() + 1];
        strcpy(argv_c[0], loc->cgi_path.c_str());
        argv_c[1] = new char[script_filename.length() + 1];
        strcpy(argv_c[1], script_filename.c_str());
        argv_c[2] = NULL;

        std::vector<std::string> env_vars_str;
        env_vars_str.push_back("REQUEST_METHOD=" + req.getMethod());
        env_vars_str.push_back("SCRIPT_FILENAME=" + script_filename);
        env_vars_str.push_back("SCRIPT_NAME=" + script_name_uri);
        env_vars_str.push_back("QUERY_STRING=" + req.getQueryString());
        env_vars_str.push_back("SERVER_PROTOCOL=HTTP/1.1");
        env_vars_str.push_back("SERVER_SOFTWARE=webserv/1.0");
        env_vars_str.push_back("GATEWAY_INTERFACE=CGI/1.1");
        if (req.getMethod() == "POST") {
            env_vars_str.push_back("CONTENT_LENGTH=" + req.getHeader("Content-Length"));
            env_vars_str.push_back("CONTENT_TYPE=" + req.getHeader("Content-Type"));
        }

        char** envp_c = new char*[env_vars_str.size() + 1];
        for (size_t i = 0; i < env_vars_str.size(); ++i) {
            envp_c[i] = new char[env_vars_str[i].length() + 1];
            strcpy(envp_c[i], env_vars_str[i].c_str());
        }
        envp_c[env_vars_str.size()] = NULL;

        execve(argv_c[0], argv_c, envp_c);

        perror("execve failed");
        for (size_t i = 0; i < env_vars_str.size(); ++i) delete[] envp_c[i];
        delete[] envp_c;
        for (size_t i = 0; i < 2; ++i) delete[] argv_c[i];
        delete[] argv_c;
        exit(EXIT_FAILURE);

    } else { // Parent Process
        close(cgi_stdout_pipe[1]); // Parent doesn't write to CGI's stdout
        close(cgi_stdin_pipe[0]);  // Parent doesn't read from CGI's stdin

        // Set the CGI's stdout READ end to non-blocking
        if (fcntl(cgi_stdout_pipe[0], F_SETFL, O_NONBLOCK) < 0) {
            std::cerr << "CGI Error: fcntl() on stdout pipe failed" << std::endl; fflush(stderr);
            kill(pid, SIGKILL); close(cgi_stdout_pipe[0]); close(cgi_stdin_pipe[1]);
            _sendErrorResponse(client, 500, "Internal Server Error: fcntl() failed", loc);
            return;
        }

        // Set the CGI's stdin WRITE end to non-blocking
        if (fcntl(cgi_stdin_pipe[1], F_SETFL, O_NONBLOCK) < 0) {
            std::cerr << "CGI Error: fcntl() on stdin pipe failed" << std::endl; fflush(stderr);
            kill(pid, SIGKILL); close(cgi_stdout_pipe[0]); close(cgi_stdin_pipe[1]);
            _sendErrorResponse(client, 500, "Internal Server Error: fcntl() failed", loc);
            return;
        }

        client->setCgiPipeFd(cgi_stdout_pipe[0]);
        client->setCgiPid(pid);
        _pipe_to_client_map[cgi_stdout_pipe[0]] = client->getFd();

        // Track CGI start time and timeout
        _cgi_start_times[client->getFd()] = time(NULL);
        _cgi_timeouts[client->getFd()] = loc ? loc->cgi_timeout : 10; // Default 10 seconds

        FD_SET(cgi_stdout_pipe[0], &_master_set);
        if (cgi_stdout_pipe[0] > _max_fd) _max_fd = cgi_stdout_pipe[0];
        close(cgi_stdin_pipe[0]);

        const HttpRequest& req = client->getRequest();

        std::string root = _config.getRoot();
        if (req.getMethod() == "POST" && !req.getBody().empty()) {
            _cgi_stdin_pipe_to_client_map[cgi_stdin_pipe[1]] = client->getFd();
            FD_SET(cgi_stdin_pipe[1], &_write_fds);
            if (cgi_stdin_pipe[1] > _max_fd) _max_fd = cgi_stdin_pipe[1];
        } else {
            close(cgi_stdin_pipe[1]); // No body to write, close it
        }
    }
}

void Server::_handleCgiRead(int pipe_fd) {
    char buffer[4096];
    ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1);

    int client_fd = _pipe_to_client_map[pipe_fd];
    if (_clients.find(client_fd) == _clients.end()) {
        // Client connection already closed, clean up CGI process
        pid_t cgi_pid = 0;
        for (std::map<int, ClientConnection*>::iterator it = _clients.begin(); it != _clients.end(); ++it) {
            if (it->second->getCgiPipeFd() == pipe_fd) {
                cgi_pid = it->second->getCgiPid();
                break;
            }
        }
        if (cgi_pid > 0) {
            waitpid(cgi_pid, NULL, 0); // Wait for child
        }
        close(pipe_fd);
        FD_CLR(pipe_fd, &_master_set);
        _pipe_to_client_map.erase(pipe_fd);
        return;
    }
    ClientConnection* client = _clients[client_fd];
    const LocationConfig* loc = client->getCgiLocation(); // Retrieve location config

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string current_response = client->getResponseBuffer();
        current_response.append(buffer);
        client->setResponse(current_response);
    } else { // read <= 0 means EOF or error
        // CGI script has finished or error occurred
        std::string cgi_output = client->getResponseBuffer();
        
        // --- NEW ERROR DETECTION LOGIC FOR CHILD PROCESS ERRORS ---
        if (cgi_output.find("execve failed") != std::string::npos ||
            cgi_output.find("No such file or directory") != std::string::npos ||
            cgi_output.find("Permission denied") != std::string::npos ||
            cgi_output.empty()) // If child produced no output, it might be an error
        {
            _sendErrorResponse(client, 500, "Internal Server Error: CGI script execution failed", loc);
            // Cleanup CGI process
            waitpid(client->getCgiPid(), NULL, 0); // Wait for child process to finish
            close(pipe_fd);
            FD_CLR(pipe_fd, &_master_set);
            _pipe_to_client_map.erase(pipe_fd);
            _cgi_start_times.erase(client_fd);
            _cgi_timeouts.erase(client_fd);
            client->setCgiPid(0);
            client->setCgiPipeFd(-1);
            return; // Important: return after sending error
        }
        // --- END NEW ERROR DETECTION LOGIC ---

        HttpResponse res;
        res.setStatusCode(200, "OK"); // Default status for successful CGI

        size_t header_end = cgi_output.find("\r\n\r\n");
        size_t separator_len = 4;
        if (header_end == std::string::npos) {
            header_end = cgi_output.find("\n\n");
            separator_len = 2;
        }

        std::string final_body;

        if (header_end == std::string::npos) {
            final_body = cgi_output;
        } else {
            std::string cgi_headers_str = cgi_output.substr(0, header_end);
            std::string cgi_body = cgi_output.substr(header_end + separator_len);
            final_body = cgi_body;

            std::stringstream ss_headers(cgi_headers_str);
            std::string line;
            while (std::getline(ss_headers, line, '\r')) { // Read line by line, split by \r
                if (line.empty()) continue;
                if (line[0] == '\n') line = line.substr(1); // Remove leading \n if present
                size_t colon_pos = line.find(":");
                if (colon_pos != std::string::npos) {
                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);
                    // Trim whitespace from value
                    size_t first_char = value.find_first_not_of(" \t");
                    if (first_char != std::string::npos) {
                        value = value.substr(first_char);
                    }
                    res.addHeader(key, value);
                }
            }
        }
        res.setBody(final_body);
        std::stringstream ss_len;
        ss_len << final_body.length();
        res.addHeader("Content-Length", ss_len.str());

        client->setResponse(res.toString());
        FD_SET(client_fd, &_write_fds);

        // Cleanup CGI process
        waitpid(client->getCgiPid(), NULL, 0); // Wait for child process to finish
        close(pipe_fd);
        FD_CLR(pipe_fd, &_master_set);
        _pipe_to_client_map.erase(pipe_fd);
        _cgi_start_times.erase(client_fd);
        _cgi_timeouts.erase(client_fd);
        client->setCgiPid(0);
        client->setCgiPipeFd(-1);
    }
}

void Server::_handleCgiWrite(int pipe_fd) {
    int client_fd = _cgi_stdin_pipe_to_client_map[pipe_fd];
    if (_clients.find(client_fd) == _clients.end()) {
        close(pipe_fd);
        FD_CLR(pipe_fd, &_write_fds);
        _cgi_stdin_pipe_to_client_map.erase(pipe_fd);
        return;
    }
    ClientConnection* client = _clients[client_fd];
    const HttpRequest& req = client->getRequest();
    const std::string& body = req.getBody();

    if (body.empty()) {
        close(pipe_fd);
        FD_CLR(pipe_fd, &_write_fds);
        _cgi_stdin_pipe_to_client_map.erase(pipe_fd);
        return;
    }

    ssize_t bytes_written = write(pipe_fd, body.c_str(), body.length());

    if (bytes_written < 0) {
        // Error handling: could be EAGAIN or a real error
        std::cerr << "CGI stdin write error" << std::endl; fflush(stderr);
        close(pipe_fd);
        FD_CLR(pipe_fd, &_write_fds);
        _cgi_stdin_pipe_to_client_map.erase(pipe_fd);
        return;
    }

    // In a more robust implementation, you would handle partial writes.
    // For now, we assume the whole body is written at once.
    std::cout << "Wrote " << bytes_written << " bytes to CGI stdin" << std::endl;
    close(pipe_fd);
    FD_CLR(pipe_fd, &_write_fds);
    _cgi_stdin_pipe_to_client_map.erase(pipe_fd);
}

void Server::_handleClientWrite(int client_fd) {
    if (_clients.find(client_fd) == _clients.end()) {
        FD_CLR(client_fd, &_write_fds); return;
    }
    ClientConnection* client = _clients[client_fd];
    const std::string& response = client->getResponseBuffer();

    if (response.empty()) {
        FD_CLR(client_fd, &_write_fds); return;
    }

    ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);

    if (bytes_sent < 0) {
        close(client_fd); FD_CLR(client_fd, &_master_set); FD_CLR(client_fd, &_write_fds); delete _clients[client_fd]; _clients.erase(client_fd); return;
    }

    if (static_cast<size_t>(bytes_sent) < response.length()) {
        client->setResponse(response.substr(bytes_sent));
    } else {
        client->clearResponseBuffer();
        FD_CLR(client_fd, &_write_fds);
        // Do NOT close the client_fd here. Keep it open for subsequent requests.
        // The client connection will be closed by _handleClientData if readRequest() returns 0 or an error occurs.
    }
}

void Server::_sendErrorResponse(ClientConnection* client, int code, const std::string& message, const LocationConfig* loc) {
    HttpResponse res;
    res.setStatusCode(code, message);
    res.addHeader("Content-Type", "text/html");

    std::string body;
    std::string custom_error_page_path;

    // 1. Check for location-specific error page
    if (loc && loc->error_pages.count(code)) {
        custom_error_page_path = loc->error_pages.at(code);
    } 
    // 2. If not found, check for server-level error page
    else if (_config.getErrorPages().count(code)) {
        custom_error_page_path = _config.getErrorPages().at(code);
    }

    if (!custom_error_page_path.empty()) {
        std::string full_path = _config.getRoot() + custom_error_page_path;
        std::ifstream custom_file(full_path.c_str());
        if (custom_file.is_open()) {
            std::stringstream buffer;
            buffer << custom_file.rdbuf();
            body = buffer.str();
        }
    }

    if (body.empty()) { // 3. Fallback to generic HTML
        std::stringstream body_ss;
        body_ss << "<html><body><h1>" << code << " " << message << "</h1></body></html>";
        body = body_ss.str();
    }

    std::stringstream len_ss;
    len_ss << body.length();
    res.addHeader("Content-Length", len_ss.str());
    res.setBody(body);

    client->setResponse(res.toString());
    FD_SET(client->getFd(), &_write_fds);
}

void Server::_checkCgiTimeouts() {
    time_t current_time = time(NULL);
    
    // Create a list of client FDs to check (avoid modifying map during iteration)
    std::vector<int> client_fds_to_check;
    for (std::map<int, time_t>::iterator it = _cgi_start_times.begin(); 
         it != _cgi_start_times.end(); ++it) {
        client_fds_to_check.push_back(it->first);
    }
    
    // Check each CGI process for timeout
    for (size_t i = 0; i < client_fds_to_check.size(); ++i) {
        int client_fd = client_fds_to_check[i];
        
        // Check if client still exists
        if (_clients.find(client_fd) == _clients.end()) {
            _cgi_start_times.erase(client_fd);
            _cgi_timeouts.erase(client_fd);
            continue;
        }
        
        ClientConnection* client = _clients[client_fd];
        time_t start_time = _cgi_start_times[client_fd];
        int timeout = _cgi_timeouts[client_fd];
        time_t elapsed = current_time - start_time;
        
        // Check if timeout exceeded
        if (elapsed > timeout) {
            std::cerr << "CGI timeout exceeded for client " << client_fd 
                      << " (elapsed: " << elapsed << "s, timeout: " << timeout << "s)" << std::endl;
            fflush(stderr);
            
            // Get the pipe fd associated with this client
            int pipe_fd = -1;
            for (std::map<int, int>::iterator it = _pipe_to_client_map.begin(); 
                 it != _pipe_to_client_map.end(); ++it) {
                if (it->second == client_fd) {
                    pipe_fd = it->first;
                    break;
                }
            }
            
            // Kill the CGI process
            pid_t cgi_pid = client->getCgiPid();
            if (cgi_pid > 0) {
                std::cout << "Killing CGI process with PID " << cgi_pid << " due to timeout" << std::endl;
                kill(cgi_pid, SIGKILL);
                waitpid(cgi_pid, NULL, WNOHANG); // Non-blocking wait
            }
            
            // Send timeout error response
            _sendErrorResponse(client, 504, "Gateway Timeout", client->getCgiLocation());
            
            // Cleanup
            if (pipe_fd >= 0) {
                close(pipe_fd);
                FD_CLR(pipe_fd, &_master_set);
                _pipe_to_client_map.erase(pipe_fd);
            }
            
            // Cleanup CGI stdin pipe if exists
            for (std::map<int, int>::iterator it = _cgi_stdin_pipe_to_client_map.begin(); 
                 it != _cgi_stdin_pipe_to_client_map.end(); ) {
                if (it->second == client_fd) {
                    close(it->first);
                    FD_CLR(it->first, &_write_fds);
                    _cgi_stdin_pipe_to_client_map.erase(it++);
                } else {
                    ++it;
                }
            }
            
            // Clear timeout tracking
            _cgi_start_times.erase(client_fd);
            _cgi_timeouts.erase(client_fd);
            client->setCgiPid(0);
            client->setCgiPipeFd(-1);
        }
    }
}
