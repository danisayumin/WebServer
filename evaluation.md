# Avaliação do Projeto Webserv: Parte Obrigatória

Este documento serve como um guia de avaliação para o projeto Webserv, explicando como a implementação atual atende a cada um dos critérios obrigatórios da avaliação, com referências diretas ao código-fonte.

### Instalação do Siege
Para os testes de estresse, o `siege` pode ser instalado via Homebrew com o comando:
```bash
brew install siege
```

### Noções básicas de um servidor HTTP

Um servidor HTTP é um software que aguarda requisições de clientes (como navegadores) através do protocolo HTTP. O fluxo básico implementado é:
1.  **Listen**: O servidor abre um socket em uma porta e "escuta" por novas conexões.
2.  **Accept**: Ao receber uma conexão, o servidor a "aceita", criando um novo socket para comunicação.
3.  **Request/Parse**: O cliente envia uma requisição, e o servidor lê e interpreta ("parse") essa requisição.
4.  **Process/Response**: O servidor processa a requisição (lê um arquivo, executa um CGI) e monta uma resposta HTTP, que é enviada de volta.
5.  **Keep-Alive**: A conexão é mantida aberta para requisições futuras, sendo um ponto crucial da eficiência do servidor.

Nosso servidor implementa este fluxo de forma **não-bloqueante** para gerenciar múltiplos clientes e requisições simultaneamente.

### Qual função foi usada para I/O Multiplexing?

Utilizamos a função `select()`. Ela é a tecnologia central que nos permite monitorar múltiplos file descriptors (sockets e pipes) de forma eficiente, sem bloquear o processo principal do servidor.

### Explicação de como `select()` funciona

A função `select()` monitora conjuntos de file descriptors (`fd_set`) para determinar se estão prontos para operações de I/O.
1.  **Preparação**: Antes de chamar `select()`, preparamos um `fd_set` para leitura (`read_fds`) e outro para escrita (`write_fds`), adicionando os FDs de interesse com `FD_SET()`.
2.  **Bloqueio e Espera**: `select()` bloqueia a execução até que um FD em um dos conjuntos esteja pronto para a operação correspondente (ou um timeout ocorra, que no nosso caso é infinito).
3.  **Retorno e Verificação**: Ao retornar, `select()` modifica os conjuntos para conter apenas os FDs que estão prontos. Usamos `FD_ISSET()` para verificar quais FDs estão sinalizados e então executar a ação apropriada (ler, escrever ou aceitar).

### O servidor usa apenas um `select()` para gerenciar leitura e escrita?

Sim, usamos **apenas uma chamada a `select()`** no loop principal, e ela monitora FDs para leitura e escrita **ao mesmo tempo**, o que é um requisito crítico.

**Onde está no código?**
A chamada na função `Server::run()` em `Server.cpp` mostra ambos os conjuntos (`read_fds` e `write_fds`) sendo passados simultaneamente, garantindo que o servidor possa reagir a qualquer tipo de evento de I/O em cada ciclo.

*   **Arquivo**: `Server.cpp`
*   **Função**: `Server::run()`

```cpp
void Server::run() {
    while (true) {
        fd_set read_fds = _master_set;
        fd_set write_fds = _write_fds; // Prepara ambos os conjuntos

        // Passa read_fds e write_fds para o select() na mesma chamada
        if (select(_max_fd + 1, &read_fds, &write_fds, NULL, NULL) < 0) {
            perror("select");
            continue;
        }

        // Itera para checar os FDs que ficaram prontos
        for (int fd = 0; fd <= _max_fd; ++fd) {
            if (FD_ISSET(fd, &read_fds)) { // Checa FDs de leitura
                // ...
            }
            if (FD_ISSET(fd, &write_fds)) { // Checa FDs de escrita
                // ...
            }
        }
    }
}
```

### Há apenas uma leitura ou escrita por cliente por chamada `select()`?

Sim. O design garante isso. Após o `select()` indicar que um FD está pronto, a função correspondente (`_handleClientData` ou `_handleClientWrite`) é chamada **uma única vez**. Essa função, por sua vez, realiza **uma única chamada de sistema** de `read()` ou `send()`.

**Onde está no código?**

*   **Arquivo**: `ClientConnection.cpp`
*   **Função**: `ClientConnection::readRequest()`

```cpp
ssize_t ClientConnection::readRequest() {
    char buffer[4096];
    // Apenas uma chamada a read() por execução da função
    ssize_t bytes_read = read(_fd, buffer, sizeof(buffer));
    // ...
    return bytes_read;
}
```

*   **Arquivo**: `Server.cpp`
*   **Função**: `Server::_handleClientWrite()`

```cpp
void Server::_handleClientWrite(int client_fd) {
    // ...
    // Apenas uma chamada a send() por execução da função
    ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
    // ...
}
```

### Verificação de erros em `read`/`recv`/`write`/`send` e remoção do cliente

Sim, o valor de retorno de todas as chamadas de I/O em sockets é rigorosamente verificado. Se um erro irrecuperável ou uma desconexão (`EOF`) ocorrer, o cliente é imediatamente removido do servidor.

*   **Leitura (`read`)**: Se `read()` retorna `0` (o cliente fechou a conexão) ou `-1` (erro), a conexão é terminada.

    *   **Arquivo**: `Server.cpp`
    *   **Função**: `Server::_handleClientData()`

    ```cpp
    void Server::_handleClientData(int client_fd) {
        if (client->readRequest() > 0) {
            // Sucesso: processa os dados
        } else { // Erro ou desconexão: read() retornou 0 ou -1
            close(client_fd);
            FD_CLR(client_fd, &_master_set);
            FD_CLR(client_fd, &_write_fds);
            delete _clients[client_fd];
            _clients.erase(client_fd);
        }
    }
    ```

*   **Escrita (`send`)**: Se `send()` retorna `-1`, a conexão é terminada. O caso de escrita parcial (`bytes_sent < response.length()`) também é tratado, mantendo o restante dos dados no buffer para a próxima iteração, sem remover o cliente.

    *   **Arquivo**: `Server.cpp`
    *   **Função**: `Server::_handleClientWrite()`

    ```cpp
    void Server::_handleClientWrite(int client_fd) {
        // ...
        ssize_t bytes_sent = send(client_fd, response.c_str(), response.length(), 0);

        if (bytes_sent < 0) { // Erro irrecuperável
            close(client_fd);
            FD_CLR(client_fd, &_master_set);
            FD_CLR(client_fd, &_write_fds);
            delete _clients[client_fd];
            _clients.erase(client_fd);
            return;
        }
        // ...
    }
    ```

### Verificação de `errno` após `read`/`recv`/`write`/`send`

**Não verificamos `errno`** para `EAGAIN` ou `EWOULDBLOCK` após `read()` ou `send()`, e isso é **proposital e correto**. Em um design com sockets não-bloqueantes, `select()` já nos garante que a operação não bloqueará. Um retorno de `-1` nessas condições é tratado como um erro de conexão genuíno, e a conexão é terminada. A verificação de `errno` é feita em outras chamadas de sistema onde é apropriado (ex: `socket`, `bind`, `listen`).

### Toda I/O em file descriptors passa por `select()`

Sim, este princípio é estritamente seguido. Nenhuma operação de I/O em sockets ou pipes ocorre fora das funções (`_handleClientData`, `_handleClientWrite`, etc.) que são chamadas apenas após a verificação `FD_ISSET` no loop principal `Server::run()`. Isso garante que nenhuma chamada de sistema de I/O bloqueie o servidor.

### Compilação do Projeto

O projeto compila de forma limpa, sem erros ou warnings, usando o `Makefile` fornecido, que adere às flags (`-Wall -Wextra -Werror`) e ao padrão C++98 exigidos.

## Configuração

O arquivo de configuração (`webserv.conf`) e seu parser (`ConfigParser.cpp`) suportam uma variedade de diretivas para controlar o comportamento do servidor.

### Múltiplos servidores com portas diferentes
Sim, é possível configurar múltiplos servidores em portas distintas. O parser lê múltiplos blocos `server {}` e o servidor cria um socket de escuta para cada porta especificada.

*   **Exemplo de Configuração**:
    ```nginx
    server {
        listen 8080;
        # ... config para o servidor 1 ...
    }
    server {
        listen 8081;
        # ... config para o servidor 2 ...
    }
    ```
*   **Código Relevante**: `Server.cpp`
    ```cpp
    // No construtor Server::Server()
    for (size_t i = 0; i < ports.size(); ++i) {
        int fd = _setupServerSocket(ports[i]);
        _listen_fds.push_back(fd);
        FD_SET(fd, &_master_set);
        if (fd > _max_fd) _max_fd = fd;
    }
    ```

### Múltiplos servidores com hostnames diferentes
Sim, implementado. O servidor agora suporta virtual hosting, permitindo que múltiplos blocos `server` compartilhem a mesma porta, mas sejam diferenciados pelo cabeçalho `Host` da requisição HTTP.

*   **Exemplo de Configuração**:
    ```nginx
    server {
        listen 8080;
        server_name localhost;
        root ./www;
        # ...
    }
    server {
        listen 8080;
        server_name example.com;
        root ./www_example;
        # ...
    }
    ```
*   **Código Relevante**: `Server.cpp`, na função `Server::_findServerConfig()`. Esta função é responsável por inspecionar o cabeçalho `Host` da requisição e a porta de escuta para selecionar o `ServerConfig` apropriado.
    ```cpp
    const ServerConfig* Server::_findServerConfig(int port, const std::string& host_header) const {
        // ... lógica para encontrar o server_config baseado na porta e no host_header ...
    }
    ```

### Página de erro padrão
Sim, o servidor pode servir páginas de erro customizadas.

*   **Exemplo de Configuração**: `error_page 404 /404.html;`
*   **Código Relevante**: `Server.cpp`, na função `_sendErrorResponse()`. A função procura pela página de erro configurada para o código de status específico e, se não encontrar, gera uma página de erro genérica.
    ```cpp
    void Server::_sendErrorResponse(...) {
        // ...
        if (loc && loc->error_pages.count(code)) {
            custom_error_page_path = loc->error_pages.at(code);
        } else if (_config.getErrorPages().count(code)) {
            custom_error_page_path = _config.getErrorPages().at(code);
        }
        // ... serve o arquivo ou gera uma página padrão
    }
    ```

### Limite de corpo do cliente
Sim, é possível limitar o tamanho do corpo de uma requisição.

*   **Exemplo de Configuração**: `client_max_body_size 10M;` (dentro de um bloco `location`).
*   **Código Relevante**: `Server.cpp`, na função `_handleClientData()`. O código compara o tamanho do corpo da requisição recebida com o limite configurado e retorna um erro `413 Payload Too Large` se o limite for excedido.
    ```cpp
    // Em _handleClientData()
    if (client->getRequestBufferSize() > max_body_size) {
        _sendErrorResponse(client, 413, "Payload Too Large", matched_location);
        client->replaceParser();
        return;
    }
    ```

### Upload de Arquivos
Sim, o servidor suporta o upload de arquivos via requisições POST multipart/form-data. A diretiva `upload_path` dentro de um bloco `location` especifica o diretório onde os arquivos enviados serão salvos.

*   **Exemplo de Configuração**:
    ```nginx
    location /upload_here {
        allow_methods POST;
        upload_path ./www/uploads;
        client_max_body_size 10M;
    }
    ```
*   **Código Relevante**: `Server.cpp`, na função `Server::_handleClientData()`. Esta função processa requisições POST, verifica o `Content-Type` para `multipart/form-data`, extrai os arquivos enviados e os salva no `upload_path` configurado, após verificar a existência e permissões de escrita do diretório.
    ```cpp
    // Em _handleClientData()
    if (req.getMethod() == "POST") {
        if (matched_location && !matched_location->upload_path.empty()) {
            // ... lógica para processar e salvar arquivos enviados ...
        }
    }
    ```

### Rotas para diretórios diferentes
Sim, o servidor pode mapear diferentes URIs para diferentes diretórios no sistema de arquivos usando blocos `location`.

*   **Exemplo de Configuração**: `location /images/ { root /var/www/image_assets; }`
*   **Código Relevante**: `Server.cpp`, em `_handleClientData()`. A lógica de matching de `location` encontra a rota correspondente e usa a diretiva `root` para construir o caminho do arquivo a ser servido.

### Arquivo padrão para diretórios
Sim, é possível definir um arquivo de índice padrão para ser servido quando um diretório é requisitado.

*   **Exemplo de Configuração**: `index main.html;` (dentro de um bloco `location` ou `server`).
*   **Código Relevante**: `Server.cpp`, em `_handleClientData()`. Se a URI da requisição é `/`, o servidor tenta servir o arquivo especificado na diretiva `index`.

### Métodos aceitos por rota
Sim, é possível restringir quais métodos HTTP são permitidos para uma rota específica.

*   **Exemplo de Configuração**: `location /api/ { allow_methods POST DELETE; }`
*   **Código Relevante**: `Server.cpp`, em `_handleClientData()`. O código verifica se o método da requisição está na lista de `allowed_methods` da `location` e retorna um erro `405 Method Not Allowed` se não estiver.
    ```cpp
    // Em _handleClientData()
    if (matched_location && !matched_location->allowed_methods.empty()) {
        // ... loop para verificar se o método é permitido ...
        if (!method_allowed) {
            _sendErrorResponse(client, 405, "Method Not Allowed", matched_location);
            client->replaceParser();
            return;
        }
    }
    ```

## Verificações Básicas e Testes

Esta seção detalha a implementação e os métodos de teste para as funcionalidades HTTP básicas do servidor.

### 1. GET, POST e DELETE requests should work.
*   **Avaliação:** Seu `webserv` suporta esses métodos HTTP para manipulação de recursos.
*   **Como testar:**
    *   **GET Request (Obter uma página):**
        *   **Exemplo:** Obter a página inicial do `localhost`.
        ```bash
        curl -v http://localhost:8080/
        ```
        *   **O que esperar:** Um código de status `200 OK` e o conteúdo HTML da página `www/index.html`.
    *   **POST Request (Enviar dados/Upload de arquivo):**
        *   **Exemplo:** Fazer upload de um arquivo para a rota `/upload_here`.
        *   Primeiro, crie um arquivo de teste:
        ```bash
        echo "Conteudo do meu arquivo de teste." > meu_arquivo.txt
        ```
        *   Agora, envie-o para o servidor:
        ```bash
        curl -v -X POST -H "Host: localhost" -F "file=@meu_arquivo.txt" http://localhost:8080/upload_here
        ```
        *   **O que esperar:** Um código de status `200 OK` e uma mensagem de sucesso do servidor, indicando que o arquivo foi salvo. Você pode verificar a existência do arquivo em `www/uploads/meu_arquivo.txt`.
    *   **DELETE Request (Remover um arquivo):**
        *   **Exemplo:** Remover o arquivo que você acabou de fazer upload.
        ```bash
        curl -v -X DELETE -H "Host: localhost" http://localhost:8080/uploads/meu_arquivo.txt
        ```
        *   **O que esperar:** Um código de status `200 OK` e uma mensagem de sucesso do servidor, indicando que o arquivo foi removido. Você pode verificar que o arquivo não existe mais em `www/uploads/`.

### 2. UNKNOWN requests should not result in a crash.
*   **Avaliação:** Seu `webserv` é robusto e foi projetado para não "crashar" (terminar inesperadamente) mesmo ao receber requisições malformadas ou com métodos HTTP desconhecidos. Ele deve retornar um código de erro apropriado.
*   **Como testar:**
    *   **Exemplo:** Enviar uma requisição com um método HTTP inválido (ex: `PATCH`).
        ```bash
        curl -v -X PATCH -H "Host: localhost" http://localhost:8080/
        ```
        *   **O que esperar:** Um código de status `405 Method Not Allowed` (se o método não for permitido para a rota) ou `501 Not Implemented` (se o servidor não reconhecer o método). O servidor **não deve parar de funcionar**.
    *   **Exemplo:** Enviar uma requisição malformada usando `telnet`.
        *   Abra uma conexão telnet:
        ```bash
        telnet localhost 8080
        ```
        *   Digite algo inválido e pressione Enter duas vezes:
        ```
        GARBAGE / HTTP/1.1
        Host: localhost
        (pressione Enter duas vezes)
        ```
        *   **O que esperar:** O servidor deve fechar a conexão ou retornar um `400 Bad Request`. O servidor **não deve parar de funcionar**.

### 3. For every test you should receive the appropriate status code.
*   **Avaliação:** Seu `webserv` foi implementado para sempre retornar o código de status HTTP correto para cada situação (sucesso, erro do cliente, erro do servidor, redirecionamento, etc.).
*   **Como testar:** Os exemplos acima já demonstram isso. Você pode observar o código de status na saída do `curl -v` (ex: `HTTP/1.1 200 OK`, `HTTP/1.1 405 Method Not Allowed`, `HTTP/1.1 404 Not Found`).
    *   **Exemplo (Recurso não encontrado):**
        ```bash
        curl -v http://localhost:8080/pagina_que_nao_existe.html
        ```
        *   **O que esperar:** Um código de status `404 Not Found`.

### 4. Upload some file to the server and get it back.
*   **Avaliação:** A funcionalidade de upload e download de arquivos está implementada.
*   **Como testar:**
    *   **Upload (já demonstrado acima):**
        ```bash
        echo "Este é um arquivo para upload e download." > arquivo_para_teste.txt
        curl -v -X POST -H "Host: localhost" -F "file=@arquivo_para_teste.txt" http://localhost:8080/upload_here
        ```
        *   **O que esperar:** `200 OK` e mensagem de sucesso.
    *   **Download (Obter o arquivo de volta):**
        *   Após o upload, o arquivo estará em `www/uploads/arquivo_para_teste.txt`. Você pode acessá-lo via GET.
        ```bash
        curl -v http://localhost:8080/uploads/arquivo_para_teste.txt
        ```
        *   **O que esperar:** `200 OK` e o conteúdo do `arquivo_para_teste.txt` na saída.

## Verificação de CGI

Esta seção avalia a funcionalidade de execução de scripts CGI pelo servidor.

### 1. O servidor está funcionando corretamente usando um CGI.
*   **Avaliação:** Seu `webserv` é capaz de executar scripts CGI (Common Gateway Interface) e retornar suas saídas como parte da resposta HTTP.
*   **Como testar (GET):**
    *   **Exemplo:** Acessar um script CGI simples via método GET.
    *   Assumindo que você tem um script `cgi-bin/simple.py` configurado em `webserv.conf` (ex: `location /cgi/ { cgi_path /usr/bin/python; cgi_ext .py; }`).
    ```bash
    curl -v http://localhost:8080/cgi-bin/simple.py
    ```
    *   **O que esperar:** Um código de status `200 OK` e a saída gerada pelo script Python.

*   **Como testar (POST):**
    *   **Exemplo:** Enviar dados para um script CGI via método POST.
    *   Assumindo que você tem um script `cgi-bin/post_test.py` que processa dados POST.
    ```bash
    curl -v -X POST -H "Content-Type: application/x-www-form-urlencoded" -d "nome=Gemini&idade=2" http://localhost:8080/cgi-bin/post_test.py
    ```
    *   **O que esperar:** Um código de status `200 OK` e a saída do script CGI processando os dados enviados.

### 2. O CGI deve ser executado no diretório correto para acesso a arquivos de caminho relativo.
*   **Avaliação:** O servidor garante que o script CGI é executado com o diretório de trabalho atual (CWD) apropriado, permitindo que o script acesse arquivos usando caminhos relativos conforme esperado.
*   **Como testar:**
    *   **Exemplo:** Crie um script CGI (ex: `cgi-bin/read_file.py`) que tenta ler um arquivo relativo ao seu próprio diretório (ex: `cgi-bin/data.txt`).
    *   Crie `cgi-bin/data.txt` com algum conteúdo.
    *   O script `read_file.py` deve conter algo como:
        ```python
        #!/usr/bin/python
        import os
        print "Content-Type: text/plain\r\n\r\n"
        try:
            with open("data.txt", "r") as f:
                print f.read()
        except Exception as e:
            print "Erro: " + str(e)
        ```
    *   Acesse o script:
    ```bash
    curl -v http://localhost:8080/cgi-bin/read_file.py
    ```
    *   **O que esperar:** Um código de status `200 OK` e o conteúdo de `cgi-bin/data.txt`. Se o CWD não estivesse correto, o script falharia ao encontrar `data.txt`.

### 3. Teste de tratamento de erros do CGI (sem crashar o servidor).
*   **Avaliação:** O servidor deve ser robusto o suficiente para lidar com scripts CGI que falham, entram em loop infinito ou causam erros, sem que o próprio servidor `webserv` crash. Erros devem ser visíveis.
*   **Como testar:**
    *   **Exemplo (Script com erro):** Crie um script `cgi-bin/error.py` que cause um erro de execução (ex: divisão por zero, sintaxe inválida).
        ```python
        #!/usr/bin/python
        print "Content-Type: text/plain\r\n\r\n"
        print 1/0 # Isso causará um erro
        ```
    *   Acesse o script:
    ```bash
    curl -v http://localhost:8080/cgi-bin/error.py
    ```
    *   **O que esperar:** O servidor deve retornar um `500 Internal Server Error` e, idealmente, a saída de erro do script CGI (se capturada) ou uma página de erro padrão. O servidor **NÃO DEVE CRASHAR**.

    *   **Exemplo (Script com loop infinito):** Crie um script `cgi-bin/infinite_loop.py` que entre em loop infinito.
        ```python
        #!/usr/bin/python
        import time
        print "Content-Type: text/plain\r\n\r\n"
        while True:
            time.sleep(1)
            print "Looping..."
        ```
    *   Acesse o script:
    ```bash
    curl -v http://localhost:8080/cgi-bin/infinite_loop.py
    ```
    *   **O que esperar:** O servidor deve eventualmente atingir um timeout (se configurado) e retornar um `504 Gateway Timeout` ou `500 Internal Server Error`. O servidor **NÃO DEVE CRASHAR**. O processo CGI deve ser encerrado pelo servidor após o timeout.

---

**Lembre-se:** Para todos esses testes, seu servidor `webserv` deve estar **rodando** em um terminal separado (`./webserv webserv.conf`). Além disso, certifique-se de que os scripts CGI tenham permissões de execução (`chmod +x script.py`).

## Verificação com Navegador

Esta seção descreve como testar o servidor usando um navegador web, inspecionando cabeçalhos e verificando o comportamento para diferentes tipos de requisições.

### 1. Inspecionar cabeçalhos de requisição e resposta.
*   **Avaliação:** O servidor deve enviar cabeçalhos de resposta HTTP corretos e o navegador deve enviar cabeçalhos de requisição esperados.
*   **Como testar:**
    1.  Abra o navegador de referência da equipe (ex: Chrome, Firefox).
    2.  Abra as Ferramentas do Desenvolvedor (geralmente `F12` ou `Ctrl+Shift+I`).
    3.  Vá para a aba "Network" (Rede).
    4.  Acesse uma URL do seu servidor (ex: `http://localhost:8080/`).
    5.  Clique na requisição para a página principal (geralmente a primeira).
    6.  Inspecione as seções "Request Headers" (Cabeçalhos da Requisição) e "Response Headers" (Cabeçalhos da Resposta).
    *   **O que esperar:**
        *   **Request Headers:** Deve conter `Host: localhost:8080`, `User-Agent`, `Accept`, etc.
        *   **Response Headers:** Deve conter `HTTP/1.1 200 OK`, `Content-Type: text/html`, `Content-Length`, `Date`, `Server: webserv`, etc.

### 2. Compatibilidade para servir um site estático completo.
*   **Avaliação:** O servidor deve ser capaz de servir todos os arquivos necessários para um site estático (HTML, CSS, JavaScript, imagens) corretamente.
*   **Como testar:**
    1.  Acesse `http://localhost:8080/` no navegador.
    2.  Verifique se a página carrega completamente, incluindo estilos (CSS) e scripts (JS), se houver.
    3.  Inspecione a aba "Network" para garantir que todos os recursos (HTML, CSS, JS, imagens) foram carregados com `200 OK`.
    *   **O que esperar:** A página deve ser renderizada corretamente, sem erros no console do navegador e todos os recursos carregados com sucesso.

### 3. Testar uma URL errada no servidor.
*   **Avaliação:** O servidor deve retornar o código de status de erro apropriado para URLs não existentes.
*   **Como testar:**
    1.  No navegador, acesse uma URL que não existe (ex: `http://localhost:8080/pagina_inexistente.html`).
    *   **O que esperar:** O navegador deve exibir a página de erro `404 Not Found` do seu servidor.

### 4. Testar listagem de diretório (se implementado e permitido).
*   **Avaliação:** Se a listagem de diretório (`autoindex`) estiver configurada e permitida para uma `location`, o servidor deve gerar uma lista navegável dos arquivos no diretório.
*   **Como testar:**
    *   **Exemplo:** Se você tiver uma `location` configurada com `autoindex on;` (ex: `location /uploads/ { autoindex on; root ./www; }`).
    1.  Acesse `http://localhost:8080/uploads/` no navegador.
    *   **O que esperar:** Uma página HTML listando os arquivos e subdiretórios dentro de `www/uploads/`.

### 5. Testar uma URL redirecionada.
*   **Avaliação:** O servidor deve lidar corretamente com redirecionamentos HTTP (códigos 3xx).
*   **Como testar:**
    *   **Exemplo:** Se você tiver uma `location` configurada com um redirecionamento (ex: `location /antigo { redirect /novo; }`).
    1.  No navegador, acesse `http://localhost:8080/antigo`.
    *   **O que esperar:** O navegador deve ser automaticamente redirecionado para `http://localhost:8080/novo`. Na aba "Network", você verá a requisição para `/antigo` com um status `301 Moved Permanently` (ou outro 3xx) e uma nova requisição para `/novo`.

### 6. Testes gerais.
*   **Avaliação:** Realize testes adicionais conforme a criatividade da equipe, como tentar acessar arquivos com permissões restritas, arquivos muito grandes, etc.
*   **Como testar:** Use o navegador para explorar todas as funcionalidades do seu servidor, incluindo CGI, upload/download, páginas de erro customizadas, etc.

## Problemas de Porta

Esta seção aborda a configuração e o comportamento do servidor em relação ao uso de portas.

### 1. Configurar múltiplas portas e usar diferentes websites.
*   **Avaliação:** O servidor deve ser capaz de escutar em múltiplas portas e servir diferentes configurações (websites) em cada uma, ou usar virtual hosting para diferenciar sites na mesma porta.
*   **Como testar:**
    *   **Exemplo (Portas diferentes):**
        *   Modifique `webserv.conf` para ter servidores em portas diferentes:
        ```nginx
        server {
            listen 8080;
            server_name localhost;
            root ./www;
            # ...
        }
        server {
            listen 8081;
            server_name outro.com;
            root ./www_outro;
            # ...
        }
        ```
        *   Inicie o servidor: `./webserv webserv.conf`
        *   Acesse `http://localhost:8080/` e `http://localhost:8081/` (ou `http://outro.com:8081/` se configurado no `hosts` ou `curl --resolve`) no navegador ou com `curl`.
        *   **O que esperar:** Conteúdo diferente para cada porta/hostname.

    *   **Exemplo (Mesma porta com Virtual Hosting - já testado):**
        *   Seu `webserv.conf` já tem `localhost`, `example.com` e `ado` na porta `8080`.
        *   Acesse `http://localhost:8080/`, `http://example.com:8080/` e `http://ado:8080/` no navegador (após configurar o `hosts` ou usando `curl --resolve`).
        *   **O que esperar:** Conteúdo diferente para cada hostname na mesma porta.

### 2. Tentar configurar a mesma porta múltiplas vezes (deve falhar).
*   **Avaliação:** O servidor deve detectar e reportar um erro se a mesma porta for especificada múltiplas vezes em diferentes blocos `listen` dentro do mesmo `server` block, ou se tentar bindar a mesma porta que já está em uso por outra instância do servidor.
*   **Como testar:**
    *   **Exemplo (Mesma porta no mesmo bloco `server`):**
        *   Modifique `webserv.conf` para ter:
        ```nginx
        server {
            listen 8080;
            listen 8080; # Porta duplicada
            server_name localhost;
            root ./www;
            # ...
        }
        ```
        *   Inicie o servidor: `./webserv webserv.conf`
        *   **O que esperar:** O servidor deve falhar ao iniciar, reportando um erro de configuração ou de `bind()` duplicado.

    *   **Exemplo (Duas instâncias do servidor na mesma porta):**
        *   Inicie uma instância do servidor: `./webserv webserv.conf` (em um terminal).
        *   Em outro terminal, tente iniciar outra instância na mesma porta: `./webserv webserv.conf`
        *   **O que esperar:** A segunda instância deve falhar ao iniciar, reportando um erro de `bind()` (endereço já em uso).

### 3. Lançar múltiplos servidores ao mesmo tempo com configurações diferentes, mas com portas comuns.
*   **Avaliação:** Se o servidor for iniciado com um `webserv.conf` que define múltiplos blocos `server` na mesma porta, ele deve ser capaz de gerenciar todos eles através do virtual hosting. Se uma das configurações for inválida, o servidor deve lidar com isso graciosamente.
*   **Como testar:**
    *   **Exemplo (Múltiplos servidores na mesma porta - já testado):**
        *   Seu `webserv.conf` já define `localhost`, `example.com` e `ado` na porta `8080`.
        *   Inicie o servidor: `./webserv webserv.conf`
        *   Teste cada hostname via navegador ou `curl --resolve`.
        *   **O que esperar:** Todos os servidores virtuais devem funcionar corretamente.

    *   **Exemplo (Configuração inválida em um dos servidores virtuais):**
        *   Modifique um dos blocos `server` em `webserv.conf` para ter uma configuração inválida (ex: `root` para um diretório inexistente, `client_max_body_size` com valor inválido).
        *   Inicie o servidor: `./webserv webserv.conf`
        *   **O que esperar:** O servidor deve iniciar, mas ao tentar acessar o servidor virtual com a configuração inválida, ele deve retornar um erro apropriado (ex: `500 Internal Server Error`, `404 Not Found` se o root for inválido), sem afetar os outros servidores virtuais.

---

**Lembre-se:** Para todos esses testes, seu servidor `webserv` deve estar **rodando** em um terminal separado (`./webserv webserv.conf`).

## Siege & Teste de Estresse

Esta seção detalha como realizar testes de estresse no servidor usando a ferramenta `siege` e o que observar.

### 1. Usar Siege para rodar testes de estresse.
*   **Avaliação:** O servidor deve ser capaz de lidar com um grande volume de requisições e usuários simultâneos sem degradação significativa de performance ou falhas.
*   **Como testar:**
    *   **Exemplo (GET simples em página vazia):**
        *   Certifique-se de que seu `webserv.conf` tenha uma rota para uma página HTML simples e leve (ex: `index.html` em `www/`).
        *   Inicie o servidor: `./webserv webserv.conf`
        *   Em outro terminal, execute o `siege`:
        ```bash
        siege -c 25 -r 100 -b http://localhost:8080/
        ```
        *   (`-c 25`: 25 usuários concorrentes, `-r 100`: 100 repetições, `-b`: benchmark, sem atrasos).
        *   **O que esperar:** A ferramenta `siege` deve reportar um alto número de transações, baixa taxa de falhas e um tempo médio de resposta aceitável.

### 2. Disponibilidade acima de 99.5% para um GET simples em página vazia com `siege -b`.
*   **Avaliação:** O servidor deve manter uma alta taxa de sucesso mesmo sob carga.
*   **Como testar:**
    *   Execute o `siege` conforme o exemplo acima.
    *   **O que esperar:** Na saída do `siege`, a métrica "Availability" (Disponibilidade) deve ser `>= 99.50 %`.

### 3. Verificar se não há vazamento de memória (monitorar o uso de memória do processo).
*   **Avaliação:** O servidor não deve vazar memória, ou seja, seu uso de memória não deve crescer indefinidamente ao longo do tempo, mesmo sob carga contínua.
*   **Como testar:**
    1.  Inicie o servidor: `./webserv webserv.conf`
    2.  Em outro terminal, monitore o uso de memória do processo `webserv`. Você pode usar ferramentas como `top`, `htop`, `ps aux | grep webserv` e observar a coluna `RSS` (Resident Set Size) ou `MEM`.
    3.  Execute um teste de estresse prolongado com `siege -b -t 5M http://localhost:8080/` (5 minutos de teste).
    4.  Continue monitorando o uso de memória durante e após o teste.
    *   **O que esperar:** O uso de memória do processo `webserv` deve se estabilizar após um pico inicial e não deve continuar crescendo indefinidamente. Pequenas flutuações são normais, mas um crescimento constante indica um vazamento.

### 4. Verificar se não há conexões penduradas (hanging connections).
*   **Avaliação:** O servidor deve gerenciar corretamente o ciclo de vida das conexões, fechando-as quando apropriado e não deixando conexões abertas desnecessariamente.
*   **Como testar:**
    1.  Inicie o servidor: `./webserv webserv.conf`
    2.  Em outro terminal, use `netstat` ou `lsof` para monitorar as conexões abertas pelo processo `webserv`.
        ```bash
        lsof -i :8080 | grep webserv
        # ou
        netstat -an | grep 8080
        ```
    3.  Execute um teste de estresse com `siege`.
    4.  Observe o número de conexões no estado `ESTABLISHED` ou `TIME_WAIT`.
    *   **O que esperar:** O número de conexões `ESTABLISHED` deve aumentar durante o teste e diminuir após o término. O número de conexões `TIME_WAIT` pode aumentar, mas deve eventualmente diminuir. Não deve haver um acúmulo excessivo de conexões `ESTABLISHED` que não são fechadas.

### 5. Usar `siege` indefinidamente sem ter que reiniciar o servidor.
*   **Avaliação:** O servidor deve ser robusto o suficiente para operar continuamente sob carga sem a necessidade de reinícios manuais.
*   **Como testar:**
    *   Execute o `siege` com a flag `-b` e um tempo de duração longo (ex: `-t 30M` para 30 minutos, ou `-t 1H` para 1 hora) em uma URL simples.
    ```bash
    siege -b -t 30M http://localhost:8080/
    ```
    *   **O que esperar:** O servidor `webserv` deve continuar respondendo às requisições do `siege` durante todo o período do teste sem travar ou precisar ser reiniciado. A disponibilidade deve permanecer alta.

---

**Lembre-se:** Para todos esses testes, seu servidor `webserv` deve estar **rodando** em um terminal separado (`./webserv webserv.conf`).


