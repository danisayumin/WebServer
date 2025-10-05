#include "Server.hpp"
#include <iostream>
#include <exception>

int main(int argc, char **argv) {
    // 2. Verifica se recebeu exatamente um argumento (arquivo de configuração)
    if (argc != 2) {
        std::cerr << "Usage: ./webserv <configuration_file>" << std::endl;
        return 1;
    }

    try {
        // 3. Cria um objeto da classe 'Server'
        Server server;

        // 4. Chama um método 'init' no servidor
        server.init(argv[1]);

        // 5. Chama um método 'run' para iniciar o loop principal
        server.run();

    } catch (const std::exception& e) {
        // 6. Captura exceções e mostra a mensagem de erro
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
