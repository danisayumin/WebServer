#include "Server.hpp"
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ServerConfig.hpp"
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
#include <set>


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

Server::Server(const ConfigParser& config) : _max_fd(0) {
    FD_ZERO(&_master_set);
    FD_ZERO(&_write_fds);

    const std::vector<ServerConfig*>& server_configs = config.getServerConfigs();
    std::set<int> unique_ports;

    for (size_t i = 0; i < server_configs.size(); ++i) {
        ServerConfig* sc = server_configs[i];
        for (size_t j = 0; j < sc->ports.size(); ++j) {
            int port = sc->ports[j];
            _configs_by_port[port].push_back(sc);
            unique_ports.insert(port);
        }
    }

    for (std::set<int>::iterator it = unique_ports.begin(); it != unique_ports.end(); ++it) {
        int port = *it;
        int fd = _setupServerSocket(port);
        _listen_fds.push_back(fd);
        _port_by_listen_fd[fd] = port;
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
    int port = _port_by_listen_fd[listening_fd];
    int client_fd = accept(listening_fd, NULL, NULL);
    if (client_fd < 0) return;
    
    fcntl(client_fd, F_SETFL, O_NONBLOCK);
    FD_SET(client_fd, &_master_set);
    
    if (_clients.find(client_fd) != _clients.end()) {
        delete _clients[client_fd];
    }
    _clients[client_fd] = new ClientConnection(client_fd, port);

    if (client_fd > _max_fd) _max_fd = client_fd;
    
    std::cout << "New connection: " << client_fd << " on port " << port << std::endl;
}

const ServerConfig* Server::_findServerConfig(int port, const std::string& host_header) const {
    if (_configs_by_port.find(port) == _configs_by_port.end()) {
        return NULL; // Should not happen
    }

    const std::vector<ServerConfig*>& configs = _configs_by_port.at(port);
    if (configs.empty()) {
        return NULL; // Should not happen
    }

    // Extract only the hostname from "hostname:port"
    std::string host_name = host_header;
    size_t colon_pos = host_name.find(':');
    if (colon_pos != std::string::npos) {
        host_name = host_name.substr(0, colon_pos);
    }

    // Find a matching server_name
    for (size_t i = 0; i < configs.size(); ++i) {
        for (size_t j = 0; j < configs[i]->server_names.size(); ++j) {
            if (configs[i]->server_names[j] == host_name) {
                return configs[i];
            }
        }
    }

    // If no match, return the first server for that port as default
    return configs[0];
}

void Server::_handleClientData(int client_fd) {
    std::cerr << "DEBUG: _handleClientData entered for client " << client_fd << std::endl;
    if (_clients.find(client_fd) == _clients.end()) return;
    ClientConnection* client = _clients[client_fd];

    if (client->readRequest() > 0) {
        if (client->isRequestComplete()) {
            const HttpRequest& req = client->getRequest();
            
            std::string host_header = req.getHeader("Host");
            const ServerConfig* server_config = _findServerConfig(client->getPort(), host_header);
            if (!server_config) {
                _sendErrorResponse(client, 400, "Bad Request", NULL, NULL);
                client->replaceParser();
                return;
            }
            client->setServerConfig(server_config);

            const LocationConfig* matched_location = NULL;
            std::string longest_match = "";
            for (size_t i = 0; i < server_config->locations.size(); ++i) {
                if (req.getUri().rfind(server_config->locations[i]->path, 0) == 0) {
                    if (server_config->locations[i]->path.length() > longest_match.length()) {
                        longest_match = server_config->locations[i]->path;
                        matched_location = server_config->locations[i];
                    }
                }
            }
            client->setLocationConfig(matched_location);

            size_t max_body_size = server_config->client_max_body_size;
            if (matched_location && matched_location->client_max_body_size > 0) {
                max_body_size = matched_location->client_max_body_size;
            }

            if (client->getRequestBufferSize() > max_body_size) {
                _sendErrorResponse(client, 413, "Payload Too Large", server_config, matched_location);
                client->replaceParser();
                return;
            }

            HttpResponse res;

            if (matched_location && !matched_location->allowed_methods.empty()) {
                bool method_allowed = false;
                for (size_t i = 0; i < matched_location->allowed_methods.size(); ++i) {
                    if (req.getMethod() == matched_location->allowed_methods[i]) {
                        method_allowed = true;
                        break;
                    }
                }
                if (!method_allowed) {
                    _sendErrorResponse(client, 405, "Method Not Allowed", server_config, matched_location);
                    client->replaceParser();
                    return;
                }
            }

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

            if (req.getMethod() == "GET") {
                std::string root = server_config->root;
                if (matched_location && !matched_location->root.empty()) {
                    root = matched_location->root;
                }

                std::string uri = req.getUri();
                if (uri[uri.length() - 1] == '/') {
                    std::string index_file = "index.html";
                    if (matched_location && !matched_location->index.empty()) {
                        index_file = matched_location->index;
                    }
                    uri += index_file;
                }
                
                std::string filePath = root + uri;
                std::ifstream file(filePath.c_str());
                
                if (file.is_open()) {
                    std::stringstream buffer; buffer << file.rdbuf();
                    res.setStatusCode(200, "OK");
                    res.addHeader("Content-Type", getMimeType(filePath));
                    std::stringstream ss_len; ss_len << buffer.str().length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(buffer.str());
                    client->setResponse(res.toString());
                } else {
                     _sendErrorResponse(client, 404, "Not Found", server_config, matched_location);
                }
            } else if (req.getMethod() == "POST") {
                std::cerr << "DEBUG: POST request to URI: " << req.getUri() << std::endl;
                if (matched_location) {
                    std::cerr << "DEBUG: Matched location path: " << matched_location->path << std::endl;
                    std::cerr << "DEBUG: Matched location upload_path: " << matched_location->upload_path << std::endl;
                } else {
                    std::cerr << "DEBUG: No location matched for POST request." << std::endl;
                }
                // Uploads are now handled by checking if a location has an upload_path
                if (matched_location && !matched_location->upload_path.empty()) {
                    std::string content_type = req.getHeader("Content-Type");
                    std::cerr << "DEBUG: Client " << client_fd << " Received Content-Type: '" << content_type << "'" << std::endl; fflush(stderr);
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
                            std::cerr << "DEBUG: Client " << client_fd << " Resolved upload directory: " << upload_dir << std::endl; fflush(stderr);

                            // Check if directory exists and is writable
                            struct stat dir_stat;
                            if (stat(upload_dir.c_str(), &dir_stat) != 0) {
                                std::cerr << "DEBUG: Client " << client_fd << " Upload directory does not exist: " << upload_dir << " Error: " << strerror(errno) << std::endl; fflush(stderr);
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
                                std::cerr << "DEBUG: Client " << client_fd << " Upload path is not a directory: " << upload_dir << std::endl; fflush(stderr);
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
                                std::cerr << "DEBUG: Client " << client_fd << " Upload directory not writable: " << upload_dir << " Error: " << strerror(errno) << std::endl; fflush(stderr);
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
                            std::cerr << "DEBUG: Client " << client_fd << " Upload directory is valid and writable." << std::endl; fflush(stderr);

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

                                std::cerr << "DEBUG: Client " << client_fd << " Attempting to save file to: " << safe_filename << std::endl; fflush(stderr);
                                std::ofstream outfile(safe_filename.c_str(), std::ios::binary);
                                if (outfile.is_open()) {
                                    std::cerr << "DEBUG: Client " << client_fd << " File stream opened successfully for: " << safe_filename << std::endl; fflush(stderr);
                                    outfile.write(file.content.c_str(), file.content.length());
                                    outfile.close();
                                    std::cout << "Uploaded file saved to: " << safe_filename << std::endl;
                                } else {
                                    std::cerr << "DEBUG: Client " << client_fd << " Failed to open file stream for: " << safe_filename << " Error: " << strerror(errno) << std::endl; fflush(stderr);
                                    all_saved = false;
                                    break;
                                }
                            }
                            if (all_saved) {
                                res.setStatusCode(200, "OK");
                                std::string body = "File(s) uploaded successfully!";
                                res.setBody(body);
                                std::stringstream ss_len; ss_len << body.length();
                                res.addHeader("Content-Length", ss_len.str());
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
                    client->setResponse(res.toString());
                } else { // Other POST requests
                    std::cerr << "DEBUG: Matched location does NOT have a valid upload_path or no location matched." << std::endl;
                    res.setStatusCode(405, "Method Not Allowed");
                    std::string body = "Method Not Allowed for this resource.";
                    res.setBody(body);
                    std::stringstream ss_len; ss_len << body.length();
                    res.addHeader("Content-Length", ss_len.str());
                    client->setResponse(res.toString());
                }
            } else if (req.getMethod() == "DELETE") {
                std::string root = server_config->root;
                if (matched_location && !matched_location->root.empty()) {
                    root = matched_location->root;
                }
                std::string filePath = root + req.getUri();
                if (std::remove(filePath.c_str()) == 0) {
                    res.setStatusCode(200, "OK");
                    res.setBody("File deleted successfully.");
                } else {
                    res.setStatusCode(404, "Not Found");
                    res.setBody("File not found or could not be deleted.");
                }
            } else {
                 res.setStatusCode(405, "Method Not Allowed");
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
    int cgi_stdout_pipe[2];
    int cgi_stdin_pipe[2];

    if (pipe(cgi_stdout_pipe) < 0 || pipe(cgi_stdin_pipe) < 0) {
        _sendErrorResponse(client, 500, "Internal Server Error", client->getServerConfig(), loc);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(cgi_stdout_pipe[0]); close(cgi_stdout_pipe[1]);
        close(cgi_stdin_pipe[0]); close(cgi_stdin_pipe[1]);
        _sendErrorResponse(client, 500, "Internal Server Error", client->getServerConfig(), loc);
        return;
    }

    if (pid == 0) { // Child Process
        close(cgi_stdout_pipe[0]);
        close(cgi_stdin_pipe[1]);
        dup2(cgi_stdin_pipe[0], STDIN_FILENO);
        dup2(cgi_stdout_pipe[1], STDOUT_FILENO);
        dup2(cgi_stdout_pipe[1], STDERR_FILENO);
        close(cgi_stdout_pipe[1]);

        const HttpRequest& req = client->getRequest();
        const ServerConfig* sc = client->getServerConfig();
        
        std::string root = sc ? sc->root : "./www";
        if (loc && !loc->root.empty()) {
            root = loc->root;
        }

        std::string script_name_uri = req.getUri();
        size_t query_pos = script_name_uri.find('?');
        if (query_pos != std::string::npos) {
            script_name_uri = script_name_uri.substr(0, query_pos);
        }

        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        std::string script_filename = std::string(cwd) + script_name_uri;

        char** argv_c = new char*[3];
        argv_c[0] = new char[loc->cgi_path.length() + 1];
        strcpy(argv_c[0], loc->cgi_path.c_str());
        argv_c[1] = new char[script_filename.length() + 1];
        strcpy(argv_c[1], script_filename.c_str());
        argv_c[2] = NULL;

        std::vector<std::string> env_vars_str;
        env_vars_str.push_back("REQUEST_METHOD=" + req.getMethod());
        env_vars_str.push_back("SCRIPT_FILENAME=" + script_filename);
        // ... other CGI env vars ...

        char** envp_c = new char*[env_vars_str.size() + 1];
        for (size_t i = 0; i < env_vars_str.size(); ++i) {
            envp_c[i] = new char[env_vars_str[i].length() + 1];
            strcpy(envp_c[i], env_vars_str[i].c_str());
        }
        envp_c[env_vars_str.size()] = NULL;

        execve(argv_c[0], argv_c, envp_c);
        perror("execve failed");
        exit(EXIT_FAILURE);

    } else { // Parent Process
        close(cgi_stdout_pipe[1]);
        close(cgi_stdin_pipe[0]);
        fcntl(cgi_stdout_pipe[0], F_SETFL, O_NONBLOCK);
        fcntl(cgi_stdin_pipe[1], F_SETFL, O_NONBLOCK);

        client->setCgiPipeFd(cgi_stdout_pipe[0]);
        client->setCgiPid(pid);
        _pipe_to_client_map[cgi_stdout_pipe[0]] = client->getFd();
        FD_SET(cgi_stdout_pipe[0], &_master_set);
        if (cgi_stdout_pipe[0] > _max_fd) _max_fd = cgi_stdout_pipe[0];

        if (client->getRequest().getMethod() == "POST" && !client->getRequest().getBody().empty()) {
            _cgi_stdin_pipe_to_client_map[cgi_stdin_pipe[1]] = client->getFd();
            FD_SET(cgi_stdin_pipe[1], &_write_fds);
            if (cgi_stdin_pipe[1] > _max_fd) _max_fd = cgi_stdin_pipe[1];
        } else {
            close(cgi_stdin_pipe[1]);
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
    const LocationConfig* loc = client->getLocationConfig(); // FIX 1: Use new getter

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string current_response = client->getResponseBuffer();
        current_response.append(buffer);
        client->setResponse(current_response);
    } else { // read <= 0 means EOF or error
        std::string cgi_output = client->getResponseBuffer();
        
        if (cgi_output.find("execve failed") != std::string::npos ||
            cgi_output.find("No such file or directory") != std::string::npos ||
            cgi_output.find("Permission denied") != std::string::npos ||
            cgi_output.empty())
        {
            // FIX 2: Pass server_config to _sendErrorResponse
            _sendErrorResponse(client, 500, "Internal Server Error: CGI script execution failed", client->getServerConfig(), loc);
            waitpid(client->getCgiPid(), NULL, 0);
            close(pipe_fd);
            FD_CLR(pipe_fd, &_master_set);
            _pipe_to_client_map.erase(pipe_fd);
            client->setCgiPid(0);
            client->setCgiPipeFd(-1);
            return;
        }

        HttpResponse res;
        res.setStatusCode(200, "OK");

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
            final_body = cgi_output.substr(header_end + separator_len);

            std::stringstream ss_headers(cgi_headers_str);
            std::string line;
            while (std::getline(ss_headers, line, '\r')) {
                if (line.empty()) continue;
                if (line[0] == '\n') line = line.substr(1);
                size_t colon_pos = line.find(":");
                if (colon_pos != std::string::npos) {
                    std::string key = line.substr(0, colon_pos);
                    std::string value = line.substr(colon_pos + 1);
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

        waitpid(client->getCgiPid(), NULL, 0);
        close(pipe_fd);
        FD_CLR(pipe_fd, &_master_set);
        _pipe_to_client_map.erase(pipe_fd);
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
        std::cerr << "DEBUG: Client " << client_fd << " _handleClientWrite: Response buffer is empty." << std::endl; fflush(stderr);
        FD_CLR(client_fd, &_write_fds); return;
    }

    std::cerr << "DEBUG: Client " << client_fd << " _handleClientWrite: Attempting to send " << response.length() << " bytes." << std::endl; fflush(stderr);
    ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);

    if (bytes_sent < 0) {
        std::cerr << "DEBUG: Client " << client_fd << " _handleClientWrite: send() failed with error: " << strerror(errno) << std::endl; fflush(stderr);
        close(client_fd); FD_CLR(client_fd, &_master_set); FD_CLR(client_fd, &_write_fds); delete _clients[client_fd]; _clients.erase(client_fd); return;
    }

    if (static_cast<size_t>(bytes_sent) < response.length()) {
        std::cerr << "DEBUG: Client " << client_fd << " _handleClientWrite: Sent " << bytes_sent << " bytes, " << (response.length() - bytes_sent) << " remaining." << std::endl; fflush(stderr);
        client->setResponse(response.substr(bytes_sent));
    } else {
        std::cerr << "DEBUG: Client " << client_fd << " _handleClientWrite: All " << bytes_sent << " bytes sent successfully." << std::endl; fflush(stderr);
        client->clearResponseBuffer();
        FD_CLR(client_fd, &_write_fds);
        // Do NOT close the client_fd here. Keep it open for subsequent requests.
        // The client connection will be closed by _handleClientData if readRequest() returns 0 or an error occurs.
    }
}

void Server::_sendErrorResponse(ClientConnection* client, int code, const std::string& message, const ServerConfig* server_config, const LocationConfig* loc) {
    HttpResponse res;
    res.setStatusCode(code, message);
    res.addHeader("Content-Type", "text/html");

    std::string body;
    std::string custom_error_page_path;

    if (loc && loc->error_pages.count(code)) {
        custom_error_page_path = loc->error_pages.at(code);
    } else if (server_config && server_config->error_pages.count(code)) {
        custom_error_page_path = server_config->error_pages.at(code);
    }

    if (!custom_error_page_path.empty() && server_config) {
        std::string full_path = server_config->root + custom_error_page_path;
        std::ifstream custom_file(full_path.c_str());
        if (custom_file.is_open()) {
            std::stringstream buffer;
            buffer << custom_file.rdbuf();
            body = buffer.str();
        }
    }

    if (body.empty()) {
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