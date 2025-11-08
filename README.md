# Webserv (42 School Project)

## Visão Geral

Este projeto é a implementação de um servidor web HTTP/1.1 a partir do zero, utilizando C++98. O objetivo é criar um servidor não-bloqueante e de thread única, similar em conceito ao NGINX, capaz de servir conteúdo estático e, futuramente, dinâmico.

O servidor adere a um conjunto estrito de regras, como a proibição de bibliotecas externas e o uso de I/O multiplexada com `select()`.

## Status Atual

O servidor está funcional e é capaz de servir um site estático simples. Ele compila sem erros, inicia corretamente, aceita múltiplas conexões e responde a requisições `GET` básicas, incluindo o tratamento de diferentes tipos de arquivo (HTML, CSS) e erros (404, 405).

## Funcionalidades Implementadas

- [x] **Build System**: Compilação automatizada com `make`.
- [x] **Parsing de Configuração**: O servidor lê um arquivo de configuração para definir porta e diretório raiz.
- [x] **Arquitetura Não-Bloqueante**: Loop de eventos principal com `select()` para I/O multiplexada.
- [x] **Gerenciamento de Conexão**: Aceita e gerencia o ciclo de vida de conexões de clientes.
- [x] **Parsing de Requisição HTTP**: Analisa requisições para extrair método, URI, cabeçalhos e corpo.
- [x] **Método GET**: Serve arquivos estáticos (HTML, CSS, etc.).
- [x] **Método POST**:
    - Suporte a upload de arquivos (`multipart/form-data`).
    - Execução de scripts CGI passando o corpo da requisição.
- [x] **Método DELETE**: Remove recursos (arquivos) do servidor.
- [x] **CGI (Common Gateway Interface)**: Executa scripts (Python) para gerar conteúdo dinâmico para requisições GET e POST.
- [x] **Suporte a MIME Types**: Identifica e envia o `Content-Type` correto.
- [x] **Geração de Respostas de Erro**: Gera respostas para `403`, `404`, `405`, `500`, etc.

## Conceitos Fundamentais

### URI (Uniform Resource Identifier)

A **URI** é uma string de caracteres que identifica de forma única um recurso na web. Pense nela como um "endereço" ou um "identificador" para qualquer coisa que possa ser nomeada, como uma página HTML, uma imagem, ou um script.

- **URL (Uniform Resource Locator)**: É o tipo mais comum de URI. Além de identificar o recurso, ela também informa **como localizá-lo**. A URL `http://localhost:8080/index.html` é uma URI que diz: "use o protocolo `http` para acessar o recurso `/index.html` no host `localhost` na porta `8080`".

- **URN (Uniform Resource Name)**: É outro tipo de URI que apenas dá um nome único a um recurso, mas não diz como encontrá-lo (ex: `urn:isbn:0451450523`).

**No nosso projeto**, quando falamos em "URI", estamos nos referindo à parte do caminho da URL que vem depois do host e da porta (ex: `/index.html`, `/cgi-bin/script.py`). O servidor usa essa URI para decidir qual arquivo servir, qual script executar ou qual recurso deletar.

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



- [ ] **Parsing de Configuração Avançado**: Melhorar o parser para suportar todas as diretivas obrigatórias do PDF, como:

    - Múltiplas portas (`listen`).

    - Limite de tamanho do corpo da requisição (`client_max_body_size`).

    - Páginas de erro customizadas.

    - Regras por `location` (métodos aceitos, redirecionamento, etc.).

- [ ] **Suporte a `Transfer-Encoding: chunked`**: Implementar a lógica para "desagrupar" requisições enviadas em partes.

- [ ] **Robustez Geral**: Continuar testando e melhorando o tratamento de erros para todos os cenários inesperados, garantindo que o servidor nunca trave.
