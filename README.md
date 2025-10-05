# Webserv

Um servidor web HTTP/1.1 simples, não-bloqueante e de thread única, escrito em C++98. Este projeto é uma exploração de conceitos fundamentais de programação de rede, incluindo a API de Sockets e multiplexação de I/O, sem o uso de bibliotecas externas.

## Como Funciona: O Ciclo de Vida de uma Requisição

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

## Status Atual


O servidor compila e executa com sucesso. A arquitetura principal baseada em eventos está funcional. Ele é capaz de:
- Aceitar múltiplas conexões TCP simultaneamente.
- Ler dados brutos enviados pelos clientes.
- Detectar e gerenciar corretamente a desconexão de um cliente, liberando seus recursos.

O núcleo do servidor é um loop de eventos que utiliza `select()` para monitorar múltiplos sockets de forma não-bloqueante.

## Funcionalidades Implementadas

- [x] **Build System**: Compilação automatizada com `make` a partir de um `Makefile`.
- [x] **Arquitetura Não-Bloqueante**: Loop de eventos principal com `select()` para I/O multiplexada.
- [x] **Gerenciamento de Sockets**: Criação, configuração (`SO_REUSEADDR`, `O_NONBLOCK`), `bind()` e `listen()` de sockets de escuta.
- [x] **Gerenciamento de Conexões**: Aceita novas conexões e cria objetos `ClientConnection` para gerenciá-las.
- [x] **Ciclo de Vida do Cliente**: Gerencia a criação e destruição de conexões de clientes, incluindo a liberação de memória e o fechamento de file descriptors.
- [x] **Leitura de Dados**: Lê dados brutos das conexões ativas quando `select()` indica que estão prontas.
- [x] **Detecção de Desconexão**: Identifica quando um cliente fecha a conexão e remove-o do conjunto de monitoramento.

## Como Compilar e Executar

### 1. Compilação

Para compilar o projeto, basta executar o comando `make` na raiz do diretório:

```bash
make
```

Isso gerará um executável chamado `webserv`.

### 2. Execução

O servidor requer um arquivo de configuração como argumento de linha de comando (embora a lógica de parsing ainda não esteja implementada).

```bash
./webserv <caminho_para_arquivo_config>
```

### 3. Teste Rápido

Você pode testar a funcionalidade básica de conexão com `netcat` ou `telnet`.

**Terminal 1 (Servidor):**
```bash
# O arquivo de configuração pode ser qualquer coisa por enquanto
./webserv dummy.conf
# Saída esperada: Server listening on port 8080
#              Server ready. Waiting for connections...
```

**Terminal 2 (Cliente):**
```bash
nc localhost 8080
```

Ao conectar, o terminal do servidor mostrará uma mensagem de nova conexão. Se você digitar algo no terminal do cliente e pressionar Enter, o servidor reportará os bytes lidos. Fechar o cliente (`Ctrl+C` no `nc`) fará com que o servidor reporte a desconexão.

## Próximos Passos

- [ ] **Parsing de Requisição HTTP**: Implementar a lógica para analisar a requisição HTTP recebida (método, URI, headers, corpo).
- [ ] **Geração de Resposta HTTP**: Construir respostas HTTP válidas (status line, headers, corpo).
- [ ] **Parsing de Configuração**: Ler e aplicar as configurações do arquivo de configuração (portas, rotas, etc.).
- [ ] **Servir Arquivos Estáticos**: Implementar a lógica para encontrar e retornar o conteúdo de arquivos solicitados.
- [ ] **Gerenciamento de Escrita**: Integrar o conjunto de FDs de escrita no `select()` para enviar respostas grandes de forma não-bloqueante.
