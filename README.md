# Webserv (42 School Project)

## Visão Geral

Este projeto é a implementação de um servidor web HTTP/1.1 a partir do zero, utilizando C++98. O objetivo é criar um servidor não-bloqueante e de thread única, similar em conceito ao NGINX, capaz de servir conteúdo estático e, futuramente, dinâmico.

O servidor adere a um conjunto estrito de regras, como a proibição de bibliotecas externas e o uso de I/O multiplexada com `select()`.

## Status Atual

O servidor está funcional e é capaz de servir um site estático simples. Ele compila sem erros, inicia corretamente, aceita múltiplas conexões e responde a requisições `GET` básicas, incluindo o tratamento de diferentes tipos de arquivo (HTML, CSS) e erros (404, 405).

## Funcionalidades Implementadas

- [x] **Build System**: Compilação automatizada com `make`.
- [x] **Parsing de Configuração**: O servidor lê um arquivo de configuração para definir porta, diretório raiz, etc.
- [x] **Arquitetura Não-Bloqueante**: Loop de eventos principal com `select()` para I/O multiplexada, capaz de lidar com múltiplos clientes sem bloquear.
- [x] **Gerenciamento de Conexão**: Aceita e gerencia o ciclo de vida de conexões de clientes, incluindo desconexões.
- [x] **Parsing de Requisição HTTP**: Analisa requisições HTTP para extrair o método e o URI, aguardando a requisição completa (`\r\n\r\n`) antes de processar.
- [x] **Roteamento de Métodos**: Aceita apenas requisições `GET` e rejeita as demais com `405 Method Not Allowed`.
- [x] **Servir Arquivos Estáticos**: Encontra e retorna o conteúdo de arquivos solicitados (ex: `index.html`, `style.css`).
- [x] **Suporte a MIME Types**: Identifica o tipo de arquivo e envia o `Content-Type` correto (ex: `text/html`, `text/css`).
- [x] **Geração de Respostas de Erro**: Gera respostas para `404 Not Found` e `405 Method Not Allowed`.

## Como Usar e Testar

### 1. Compilação

Para compilar o projeto, use o comando `make` ou `make re` para uma recompilação limpa.

```bash
make re
```

### 2. Arquivo de Configuração (`.config`)

O servidor é configurado através de um arquivo. O formato atual é simples e suporta as seguintes diretivas dentro de um bloco `server { ... }`:

- `listen`: A porta em que o servidor vai escutar.
- `server_name`: O nome do servidor (atualmente não utilizado).
- `root`: O diretório raiz de onde os arquivos serão servidos.
- `error_page`: Define uma página customizada para um código de erro (atualmente não utilizado).

**Exemplo de `.config`:**
```nginx
server {
    listen 8080;
    server_name webserv.com;

    root ./www;
    index index.html;

    error_page 404 /404.html;

    location / {
        try_files $uri $uri/ =404;
    }

    location /upload {
        client_max_body_size 10M;
    }
}
```

### 3. Execução

Inicie o servidor passando o arquivo de configuração como argumento.

```bash
./webserv .config
```

O servidor ficará em execução, aguardando por conexões.

### 4. Testando

**a) Com o Navegador:**

Abra seu navegador e acesse `http://localhost:8080`. Você deverá ver a página de boas-vindas com o estilo CSS aplicado.

**b) Com `curl` (GET bem-sucedido):**

Abra um novo terminal e peça a página inicial. Você receberá o conteúdo do `index.html`.
```bash
curl http://localhost:8080
```

**c) Com `curl` (Método não permitido):**

Envie uma requisição `POST`. O servidor deve responder com `405 Method Not Allowed`. O `-v` mostra os detalhes da resposta.
```bash
curl -X POST -v http://localhost:8080
```

## Arquitetura e Ciclo de Vida da Requisição

Esta seção detalha a jornada completa de uma requisição HTTP, desde o navegador até a resposta do servidor.

### 1. Inicialização do Servidor

- Você executa `./webserv .config` no terminal.
- O `main` cria uma instância do `ConfigParser`, que lê e valida o `.config`.
- Em seguida, o `main` cria uma instância do `Server`, passando as configurações lidas.
- O construtor do `Server` chama `socket()`, `bind()` e `listen()`. Isso instrui o Sistema Operacional (SO) a começar a escutar por conexões na porta especificada (ex: 8080).
- **Ponto Chave**: Se o servidor não estiver rodando, o SO recusará ativamente qualquer tentativa de conexão a essa porta, resultando no erro `Connection refused`.

### 2. A Chegada da Conexão

- Um cliente (navegador ou `curl`) tenta se conectar a `localhost:8080`.
- O SO, que estava escutando, vê essa tentativa e notifica o processo `webserv`.
- Dentro do `Server::run()`, a chamada `select()` desbloqueia e reporta que o socket principal de escuta tem atividade.
- O método `_acceptNewConnection` é chamado. Ele usa `accept()` para criar um **novo socket** dedicado exclusivamente a este cliente.
- Uma instância de `ClientConnection` é criada para gerenciar este novo socket e o estado do cliente.

### 3. Recebendo e Validando a Requisição

- O cliente envia a requisição HTTP como texto (ex: `GET /style.css HTTP/1.1...`).
- O `select()` novamente desperta, desta vez reportando atividade no socket do cliente.
- `_handleClientData` é chamado. O método `client->readRequest()` lê os dados do socket e os acumula em um buffer interno.
- O `readRequest` verifica continuamente se o buffer contém o marcador de fim de cabeçalhos (`\r\n\r\n`). Enquanto não o encontra, o servidor simplesmente aguarda por mais dados.
- Quando a requisição está completa, uma instância de `HttpRequest` é criada para interpretar o buffer.
- A primeira verificação é feita: `if (req.getMethod() != "GET")`. Se o método não for `GET`, uma resposta `405 Method Not Allowed` é preparada e o processo pula para o passo 5.

### 4. Montando a Resposta (Lógica GET)

- O servidor determina o caminho do arquivo solicitado. Ele combina a diretiva `root` do `.config` (ex: `./www`) com o URI da requisição (ex: `/style.css`) para formar o caminho completo: `./www/style.css`.
- O servidor tenta abrir o arquivo. 
- **Caso de Sucesso**: Se o arquivo for encontrado, uma instância de `HttpResponse` é preenchida com:
    - Status: `200 OK`.
    - Headers: `Content-Type` (determinado pela função `getMimeType`) e `Content-Length` (o tamanho do arquivo).
    - Corpo: O conteúdo do arquivo lido.
- **Caso de Falha**: Se o arquivo não for encontrado, a `HttpResponse` é preenchida com:
    - Status: `404 Not Found`.
    - Corpo: Um HTML simples de erro.

### 5. Envio e Finalização

- O método `res.toString()` é chamado para montar a string de texto completa da resposta HTTP (status, headers e corpo).
- `send()` envia essa string de volta para o cliente através do socket da conexão.
- `close()` é chamado no socket do cliente, encerrando a conexão.
- O objeto `ClientConnection` é destruído, liberando a memória.
- O servidor volta ao seu loop com `select()`, pronto para a próxima conexão.

## Próximos Passos

- [ ] **Roteamento Avançado**: Implementar o parsing e a lógica dos blocos `location`.
- [ ] **Conteúdo Dinâmico (CGI)**: Adicionar a capacidade de executar scripts para gerar respostas.
- [ ] **Suporte a POST**: Implementar o tratamento do corpo de requisições, necessário para uploads de arquivos e formulários.
- [ ] **Gerenciamento de Escrita Não-Bloqueante**: Usar o `select()` também para escritas (`write_fds`) para lidar com o envio de arquivos grandes sem bloquear o servidor.
- [ ] **Melhorar Parsing de Configuração**: Tornar o parser mais robusto e capaz de lidar com múltiplas seções `server`.