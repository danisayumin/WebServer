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
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <signal.h>

std::string getMimeType(const std::string& filePath) {
    if (filePath.rfind(".html") != std::string::npos) return "text/html";
    if (filePath.rfind(".css") != std::string::npos) return "text/css";
    return "application/octet-stream";
}

Server::Server(const ConfigParser& config) : _config(config), _listen_fd(0), _max_fd(0) {
    FD_ZERO(&_master_set);
    FD_ZERO(&_write_fds);
    _listen_fd = _setupServerSocket(_config.getPort());
    _max_fd = _listen_fd;
}

Server::~Server() {
    if (_listen_fd > 0) close(_listen_fd);
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
    FD_SET(fd, &_master_set);
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
                if (fd == _listen_fd) {
                    _acceptNewConnection(fd);
                } else if (_pipe_to_client_map.count(fd)) {
                    _handleCgiRead(fd);
                } else if (_clients.count(fd)) {
                    _handleClientData(fd);
                }
            }
            if (FD_ISSET(fd, &write_fds)) {
                _handleClientWrite(fd);
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
        if (client->isRequestComplete()) {
            HttpRequest req(client->getRequestBuffer());
            HttpResponse res;

            const std::vector<LocationConfig*>& locations = _config.getLocations();
            const LocationConfig* matched_location = NULL;
            std::string longest_match = "";

            for (size_t i = 0; i < locations.size(); ++i) {
                if (req.getUri().rfind(locations[i]->path, 0) == 0) {
                    if (locations[i]->path.length() > longest_match.length()) {
                        longest_match = locations[i]->path;
                        matched_location = locations[i];
                    }
                }
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
            
            if (req.getMethod() != "GET") {
                res.setStatusCode(405, "Method Not Allowed");
            } else {
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
                if (file.is_open()) {
                    std::stringstream buffer; buffer << file.rdbuf();
                    res.setStatusCode(200, "OK");
                    res.addHeader("Content-Type", getMimeType(filePath));
                    std::stringstream ss_len; ss_len << buffer.str().length();
                    res.addHeader("Content-Length", ss_len.str());
                    res.setBody(buffer.str());
                } else {
                    res.setStatusCode(404, "Not Found");
                }
            }
            client->setResponse(res.toString());
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
    int cgi_pipe[2];
    if (pipe(cgi_pipe) < 0) {
        std::cerr << "CGI Error: pipe() failed" << std::endl;
        _sendErrorResponse(client, 500, "Internal Server Error: pipe() failed");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "CGI Error: fork() failed" << std::endl;
        close(cgi_pipe[0]);
        close(cgi_pipe[1]);
        _sendErrorResponse(client, 500, "Internal Server Error: fork() failed");
        return;
    }

    if (pid == 0) { // Child Process
        close(cgi_pipe[0]); // Close read end

        // Redirect stdout and stderr to pipe
        if (dup2(cgi_pipe[1], STDOUT_FILENO) == -1) {
            perror("dup2 STDOUT_FILENO failed");
            exit(EXIT_FAILURE);
        }
        if (dup2(cgi_pipe[1], STDERR_FILENO) == -1) { // Redirect stderr as well
            perror("dup2 STDERR_FILENO failed");
            exit(EXIT_FAILURE);
        }
        close(cgi_pipe[1]); // Close original write end

        HttpRequest req(client->getRequestBuffer());

        std::string root = _config.getRoot();
        if (loc && !loc->root.empty()) {
            root = loc->root;
        }

        std::string script_name_uri = req.getUri();
        size_t query_pos = script_name_uri.find('?');
        if (query_pos != std::string::npos) {
            script_name_uri = script_name_uri.substr(0, query_pos);
        }

        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd failed");
            exit(EXIT_FAILURE);
        }
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
        env_vars_str.push_back("SCRIPT_NAME=" + script_name_uri);
        env_vars_str.push_back("QUERY_STRING=" + req.getQueryString());
        env_vars_str.push_back("SERVER_PROTOCOL=HTTP/1.1");
        env_vars_str.push_back("SERVER_SOFTWARE=webserv/1.0");
        env_vars_str.push_back("GATEWAY_INTERFACE=CGI/1.1");

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
        close(cgi_pipe[1]); // Parent doesn't write to the pipe

        // Set the pipe's read end to non-blocking
        if (fcntl(cgi_pipe[0], F_SETFL, O_NONBLOCK) < 0) {
            std::cerr << "CGI Error: fcntl() failed" << std::endl;
            kill(pid, SIGKILL); // Kill the child process
            close(cgi_pipe[0]);
            _sendErrorResponse(client, 500, "Internal Server Error: fcntl() failed");
            return;
        }

        // Associate the pipe and pid with the client connection
        client->setCgiPipeFd(cgi_pipe[0]);
        client->setCgiPid(pid);
        _pipe_to_client_map[cgi_pipe[0]] = client->getFd();

        // Add the pipe's read end to the master set to be monitored by select()
        FD_SET(cgi_pipe[0], &_master_set);
        if (cgi_pipe[0] > _max_fd) {
            _max_fd = cgi_pipe[0];
        }
    }
}

void Server::_handleCgiRead(int pipe_fd) {
    char buffer[4096];
    ssize_t bytes_read = read(pipe_fd, buffer, sizeof(buffer) - 1);

    int client_fd = _pipe_to_client_map[pipe_fd];
    if (_clients.find(client_fd) == _clients.end()) {
        // Client connection already closed, clean up CGI process
        // We need to find the pid associated with this pipe_fd to waitpid
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

    if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        std::string current_response = client->getResponseBuffer();
        current_response.append(buffer);
        client->setResponse(current_response);
        std::cerr << "_handleCgiRead: Read " << bytes_read << " bytes. Current output: " << current_response << std::endl;
    } else { // read <= 0 means EOF or error
        std::cerr << "_handleCgiRead: EOF or error. bytes_read = " << bytes_read << std::endl;
        // CGI script has finished or error occurred
        std::string cgi_output = client->getResponseBuffer();
        std::cerr << "_handleCgiRead: Full CGI output: \n---\n" << cgi_output << "\n---\n" << std::endl;
        
        // --- NEW ERROR DETECTION LOGIC FOR CHILD PROCESS ERRORS ---
        if (cgi_output.find("execve failed") != std::string::npos ||
            cgi_output.find("No such file or directory") != std::string::npos ||
            cgi_output.find("Permission denied") != std::string::npos ||
            cgi_output.empty()) // If child produced no output, it might be an error
        {
            std::cerr << "_handleCgiRead: Detected child process error in CGI output. Sending 500." << std::endl;
            _sendErrorResponse(client, 500, "Internal Server Error: CGI script execution failed");
            // Cleanup CGI process
            waitpid(client->getCgiPid(), NULL, 0); // Wait for child process to finish
            close(pipe_fd);
            FD_CLR(pipe_fd, &_master_set);
            _pipe_to_client_map.erase(pipe_fd);
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
            std::cerr << "_handleCgiRead: No CGI headers found." << std::endl;
            final_body = cgi_output;
        } else {
            std::string cgi_headers_str = cgi_output.substr(0, header_end);
            std::string cgi_body = cgi_output.substr(header_end + separator_len);
            final_body = cgi_body;
            std::cerr << "_handleCgiRead: CGI Headers: \n" << cgi_headers_str << std::endl;
            std::cerr << "_handleCgiRead: CGI Body: \n" << cgi_body << std::endl;

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
                    std::cerr << "_handleCgiRead: Added Header: " << key << ": " << value << std::endl;
                }
            }
        }
        res.setBody(final_body);
        std::stringstream ss_len;
        ss_len << final_body.length();
        res.addHeader("Content-Length", ss_len.str());
        std::cerr << "_handleCgiRead: Final Body Length: " << final_body.length() << std::endl;

        client->setResponse(res.toString());
        FD_SET(client_fd, &_write_fds);

        // Cleanup CGI process
        waitpid(client->getCgiPid(), NULL, 0); // Wait for child process to finish
        close(pipe_fd);
        FD_CLR(pipe_fd, &_master_set);
        _pipe_to_client_map.erase(pipe_fd);
        client->setCgiPid(0);
        client->setCgiPipeFd(-1);
    }
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
        close(client_fd); FD_CLR(client_fd, &_master_set); delete _clients[client_fd]; _clients.erase(client_fd);
    }
}

void Server::_sendErrorResponse(ClientConnection* client, int code, const std::string& message) {
    HttpResponse res;
    res.setStatusCode(code, message);
    res.addHeader("Content-Type", "text/html");

    std::stringstream body_ss;
    body_ss << "<html><body><h1>" << code << " " << message << "</h1></body></html>";
    const std::string body = body_ss.str();

    std::stringstream len_ss;
    len_ss << body.length();
    res.addHeader("Content-Length", len_ss.str());
    res.setBody(body);

    client->setResponse(res.toString());
    FD_SET(client->getFd(), &_write_fds);
}
