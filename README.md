# Webserv

Um servidor web HTTP/1.1 simples, não-bloqueante e de thread única, escrito em C++98. Este projeto é uma exploração de conceitos fundamentais de programação de rede, incluindo a API de Sockets e multiplexação de I/O, sem o uso de bibliotecas externas.

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
