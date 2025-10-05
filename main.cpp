#include "Server.hpp"
#include "ConfigParser.hpp"
#include <iostream>
#include <exception>

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cerr << "Usage: ./webserv <configuration_file>" << std::endl;
        return 1;
    }

    try {
        // 1. Cria o objeto de configuração a partir do arquivo.
        ConfigParser config(argv[1]);

        // 2. Cria o servidor, passando o objeto de configuração.
        Server server(config);

        // 3. Inicia o loop principal do servidor.
        server.run();

    } catch (const std::exception& e) {
        std::cerr << "Critical Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
