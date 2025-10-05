#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP

#include <vector>
#include <string>

// Para ssize_t (retorno do read)
#include <unistd.h>

class ClientConnection {
public:
    // 2. Um construtor que recebe o file descriptor.
    explicit ClientConnection(int client_fd);
    ~ClientConnection();

    // 3. Um método que tentará ler dados do socket para o buffer.
    // Retorna o número de bytes lidos. 0 indica que o cliente fechou a conexão.
    // Um valor negativo indica um erro.
    ssize_t readRequest();

    // 4. Um método para obter o file descriptor.
    int getFd() const;

private:
    // Construtor de cópia e operador de atribuição privados para evitar cópias acidentais
    ClientConnection(const ClientConnection& other);
    ClientConnection& operator=(const ClientConnection& other);

    // 1. Atributos privados
    int _fd;
    std::vector<char> _read_buffer;
    std::string       _write_buffer;
};

#endif // CLIENT_CONNECTION_HPP
