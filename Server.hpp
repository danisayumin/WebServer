#ifndef SERVER_HPP
#define SERVER_HPP

// 1. Cabeçalhos padrões necessários

// Para std::cout, std::cerr, etc.
#include <iostream>
// Para std::string, std::vector, std::map
#include <string>
#include <vector>
#include <map>
#include <exception>

// Para todas as chamadas de sistema de socket (socket, bind, listen, accept)
#include <sys/socket.h>
// Para a estrutura sockaddr_in
#include <netinet/in.h>
// Para select() e macros relacionadas (FD_SET, FD_ISSET, FD_ZERO)
#include <sys/select.h>
// Para close()
#include <unistd.h>
// Para fcntl() para tornar os sockets não-bloqueantes
#include <fcntl.h>
// Para memset
#include <cstring>

// Stub para a classe de configuração
class ServerConfig {};

#include "ClientConnection.hpp"

class Server {
public:
    // 3. Métodos públicos
    Server();
    ~Server();

    void init(const char* configFile);
    void run();

private:
    // 2. Atributos privados

    // Mapeia um FD de escuta à sua configuração de servidor correspondente
    std::map<int, ServerConfig> _listening_sockets;
    // Mapeia um FD de cliente a um PONTEIRO para seu objeto de conexão
    std::map<int, ClientConnection*> _clients;

    // Conjunto mestre de file descriptors para o select()
    fd_set _master_set;
    // Conjuntos de trabalho, que são modificados pelo select()
    fd_set _read_fds;
    fd_set _write_fds;

    // O número do maior file descriptor, necessário para o select()
    int _max_fd;

    // 4. Métodos privados para a lógica interna
    void setupServerSockets();
    void acceptNewConnection(int listening_fd);
    void handleClientRequest(int client_fd); // <-- Adicionado aqui
};

#endif // SERVER_HPP
