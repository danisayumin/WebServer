# Webserv (42 School Project)

## Visão Geral

Este projeto é a implementação de um servidor web HTTP/1.1 a partir do zero, utilizando C++98. O objetivo é criar um servidor não-bloqueante e de thread única, similar em conceito ao NGINX, capaz de servir conteúdo estático e, futuramente, dinâmico.

O servidor adere a um conjunto estrito de regras, como a proibição de bibliotecas externas e o uso de I/O multiplexada com `select()`.

## Status Atual

O servidor está funcional e é capaz de servir um site estático simples. Ele compila sem erros, inicia corretamente, aceita múltiplas conexões e responde a requisições `GET` básicas, incluindo o tratamento de diferentes tipos de arquivo (HTML, CSS) e erros (404, 405).

## Funcionalidades Implementadas

- [x] **Build System**: Compilação automatizada com `make` (`-Wall -Wextra -Werror -std=c++98`).
- [x] **Arquitetura Não-Bloqueante**: Loop de eventos principal com `select()` para I/O multiplexada (única chamada para leitura e escrita).
- [x] **Gerenciamento de Conexão**: Aceita e gerencia o ciclo de vida de conexões de clientes, com verificação rigorosa de erros de I/O e remoção de clientes.
- [x] **Parsing de Requisição HTTP**: Analisa requisições para extrair método, URI, cabeçalhos e corpo.
- [x] **Métodos HTTP Suportados**:
    - [x] **GET**: Serve arquivos estáticos (HTML, CSS, JS, imagens).
    - [x] **POST**: Suporte a upload de arquivos (`multipart/form-data`) e execução de scripts CGI.
    - [x] **DELETE**: Remove recursos (arquivos) do servidor.
- [x] **CGI (Common Gateway Interface)**:
    - Executa scripts (ex: Python) para gerar conteúdo dinâmico para requisições GET e POST.
    - Garante execução no diretório correto para acesso a arquivos relativos.
    - Tratamento robusto de erros em scripts CGI (sem crashar o servidor, com retorno de `500 Internal Server Error` ou `504 Gateway Timeout`).
- [x] **Configuração Flexível via `webserv.conf`**:
    - [x] **Múltiplos Servidores**: Suporte a múltiplos blocos `server {}`.
    - [x] **Múltiplas Portas**: Escuta em portas diferentes.
    - [x] **Virtual Hosting**: Diferencia servidores pelo `server_name` (cabeçalho `Host`) na mesma porta.
    - [x] **Diretório Raiz (`root`)**: Define o diretório base para servir arquivos.
    - [x] **Páginas de Erro Customizadas (`error_page`)**: Define páginas HTML personalizadas para códigos de erro (ex: 404, 500).
    - [x] **Limite de Corpo da Requisição (`client_max_body_size`)**: Restringe o tamanho máximo do corpo das requisições.
    - [x] **Rotas Específicas (`location`)**: Mapeia URIs para diferentes configurações (diretórios, métodos permitidos, CGI, upload).
    - [x] **Arquivo Padrão para Diretórios (`index`)**: Define o arquivo a ser servido quando um diretório é acessado.
    - [x] **Métodos Permitidos por Rota (`allow_methods`)**: Restringe os métodos HTTP aceitos para uma `location` específica.
    - [x] **Caminho de Upload (`upload_path`)**: Define o diretório para salvar arquivos enviados via POST.
- [x] **Suporte a MIME Types**: Identifica e envia o `Content-Type` correto para diversos tipos de arquivo.
- [x] **Tratamento de Erros HTTP**: Gera respostas com códigos de status apropriados (ex: `200 OK`, `400 Bad Request`, `403 Forbidden`, `404 Not Found`, `405 Method Not Allowed`, `413 Payload Too Large`, `500 Internal Server Error`, `501 Not Implemented`, `504 Gateway Timeout`).
- [x] **Robustez e Estabilidade**:
    - [x] Não "crashar" com requisições desconhecidas ou malformadas.
    - [x] Gerenciamento eficiente de recursos para evitar vazamentos de memória.
    - [x] Prevenção de conexões penduradas (hanging connections).
    - [x] Capacidade de operar continuamente sob testes de estresse (`siege`).

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

### 2. Arquivo de Configuração (`webserv.conf`)

O servidor é configurado através do arquivo `webserv.conf`. Um exemplo detalhado e funcional é fornecido no repositório. Certifique-se de que ele esteja configurado para as portas e hosts que você deseja testar.

### 3. Execução

Inicie o servidor passando o arquivo de configuração como argumento.

```bash
./webserv webserv.conf
```

O servidor ficará em execução, aguardando por conexões.


