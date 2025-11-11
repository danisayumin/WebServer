# Plano de Testes para o Webserv

Este documento detalha os passos para testar as funcionalidades do servidor `webserv`, com base nas avaliações e exemplos fornecidos.

**Pré-requisitos:**
*   O servidor `webserv` deve estar compilado e executável (`./webserv webserv.conf`).
*   O arquivo de configuração `webserv.conf` deve estar configurado conforme os exemplos (ou com as configurações padrão que permitem os testes).
*   As entradas no arquivo `/etc/hosts` para `example.com` e `ado` devem estar configuradas para `127.0.0.1`.
*   Ferramentas como `curl`, `telnet` e `siege` devem estar instaladas.
*   Scripts CGI de teste (`simple.py`, `post_test.py`, `read_file.py`, `error.py`, `infinite_loop.py`) devem estar presentes no diretório `cgi-bin/` e ter permissões de execução (`chmod +x`).
*   Arquivos de teste (`meu_arquivo.txt`, `arquivo_para_teste.txt`, `cgi-bin/data.txt`) devem ser criados conforme necessário.

---

## 1. Verificações Básicas (GET, POST, DELETE, UNKNOWN, Status Codes, Upload/Download)

### 1.1. Testar Requisições GET, POST e DELETE

#### Teste 1.1.1: Requisição GET (Obter uma página)
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Abra um terminal e execute:
        ```bash
        curl -v http://localhost:8080/
        ```
*   **Resultado Esperado:**
    *   Um código de status `200 OK`.
    *   O conteúdo HTML da página `www/index.html` na saída.

#### Teste 1.1.2: Requisição POST (Enviar dados/Upload de arquivo)
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Crie um arquivo de teste:
        ```bash
        echo "Conteudo do meu arquivo de teste." > meu_arquivo.txt
        ```
    3.  Envie o arquivo para o servidor (assumindo `upload_here` está configurado em `webserv.conf`):
        ```bash
        curl -v -X POST -H "Host: localhost" -F "file=@meu_arquivo.txt" http://localhost:8080/upload_here
        ```
*   **Resultado Esperado:**
    *   Um código de status `200 OK`.
    *   Uma mensagem de sucesso do servidor.
    *   O arquivo `meu_arquivo.txt` deve existir em `www/uploads/`.

#### Teste 1.1.3: Requisição DELETE (Remover um arquivo)
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Certifique-se de que o arquivo `meu_arquivo.txt` (ou outro arquivo) exista em `www/uploads/`.
    3.  Execute:
        ```bash
        curl -v -X DELETE -H "Host: localhost" http://localhost:8080/uploads/meu_arquivo.txt
        ```
*   **Resultado Esperado:**
    *   Um código de status `200 OK`.
    *   Uma mensagem de sucesso do servidor.
    *   O arquivo `meu_arquivo.txt` não deve mais existir em `www/uploads/`.

### 1.2. Testar Requisições Desconhecidas (UNKNOWN requests)

#### Teste 1.2.1: Método HTTP inválido
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Execute:
        ```bash
        curl -v -X PATCH -H "Host: localhost" http://localhost:8080/
        ```
*   **Resultado Esperado:**
    *   Um código de status `405 Method Not Allowed` ou `501 Not Implemented`.
    *   O servidor **não deve parar de funcionar**.

#### Teste 1.2.2: Requisição malformada com Telnet
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Abra um terminal e execute:
        ```bash
        telnet localhost 8080
        ```
    3.  Digite o seguinte e pressione Enter duas vezes:
        ```
        GARBAGE / HTTP/1.1
        Host: localhost
        ```
*   **Resultado Esperado:**
    *   O servidor deve fechar a conexão ou retornar um `400 Bad Request`.
    *   O servidor **não deve parar de funcionar**.

### 1.3. Testar Códigos de Status Apropriados

#### Teste 1.3.1: Recurso não encontrado (404)
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Execute:
        ```bash
        curl -v http://localhost:8080/pagina_que_nao_existe.html
        ```
*   **Resultado Esperado:**
    *   Um código de status `404 Not Found`.

### 1.4. Testar Upload e Download de Arquivos

#### Teste 1.4.1: Upload e Download
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Crie um arquivo de teste:
        ```bash
        echo "Este é um arquivo para upload e download." > arquivo_para_teste.txt
        ```
    3.  Faça o upload do arquivo:
        ```bash
        curl -v -X POST -H "Host: localhost" -F "file=@arquivo_para_teste.txt" http://localhost:8080/upload_here
        ```
    4.  Após o upload, baixe o arquivo:
        ```bash
        curl -v http://localhost:8080/uploads/arquivo_para_teste.txt
        ```
*   **Resultado Esperado:**
    *   Para o upload: `200 OK` e mensagem de sucesso.
    *   Para o download: `200 OK` e o conteúdo de `arquivo_para_teste.txt` na saída.

---

## 2. Verificação de CGI

### 2.1. Testar CGI com método GET

#### Teste 2.1.1: CGI GET simples
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Certifique-se de que `cgi-bin/simple.py` existe e tem permissões de execução (`chmod +x cgi-bin/simple.py`).
    3.  Execute:
        ```bash
        curl -v http://localhost:8080/cgi-bin/simple.py
        ```
*   **Resultado Esperado:**
    *   Um código de status `200 OK`.
    *   A saída gerada pelo script Python.

### 2.2. Testar CGI com método POST

#### Teste 2.2.1: CGI POST com dados
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Certifique-se de que `cgi-bin/post_test.py` existe e tem permissões de execução (`chmod +x cgi-bin/post_test.py`).
    3.  Execute:
        ```bash
        curl -v -X POST -H "Content-Type: application/x-www-form-urlencoded" -d "nome=Gemini&idade=2" http://localhost:8080/cgi-bin/post_test.py
        ```
*   **Resultado Esperado:**
    *   Um código de status `200 OK`.
    *   A saída do script CGI processando os dados enviados.

### 2.3. Testar CGI com acesso a arquivos relativos

#### Teste 2.3.1: CGI lendo arquivo relativo
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Crie `cgi-bin/data.txt` com algum conteúdo.
    3.  Crie `cgi-bin/read_file.py` com o seguinte conteúdo e dê permissão de execução:
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
    4.  Execute:
        ```bash
        curl -v http://localhost:8080/cgi-bin/read_file.py
        ```
*   **Resultado Esperado:**
    *   Um código de status `200 OK`.
    *   O conteúdo de `cgi-bin/data.txt` na saída.

### 2.4. Testar tratamento de erros do CGI (sem crashar o servidor)

#### Teste 2.4.1: Script CGI com erro de execução
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Crie `cgi-bin/error.py` com o seguinte conteúdo e dê permissão de execução:
        ```python
        #!/usr/bin/python
        print "Content-Type: text/plain\r\n\r\n"
        print 1/0 # Isso causará um erro
        ```
    3.  Execute:
        ```bash
        curl -v http://localhost:8080/cgi-bin/error.py
        ```
*   **Resultado Esperado:**
    *   Um código de status `500 Internal Server Error`.
    *   O servidor **NÃO DEVE CRASHAR**.

#### Teste 2.4.2: Script CGI com loop infinito
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Crie `cgi-bin/infinite_loop.py` com o seguinte conteúdo e dê permissão de execução:
        ```python
        #!/usr/bin/python
        import time
        print "Content-Type: text/plain\r\n\r\n"
        while True:
            time.sleep(1)
            print "Looping..."
        ```
    3.  Execute:
        ```bash
        curl -v http://localhost:8080/cgi-bin/infinite_loop.py
        ```
*   **Resultado Esperado:**
    *   O servidor deve eventualmente atingir um timeout e retornar um `504 Gateway Timeout` ou `500 Internal Server Error`.
    *   O servidor **NÃO DEVE CRASHAR**.
    *   O processo CGI deve ser encerrado pelo servidor após o timeout.

---

## 3. Verificação com Navegador

### 3.1. Inspecionar cabeçalhos de requisição e resposta

#### Teste 3.1.1: Inspeção de cabeçalhos
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Abra o navegador de referência da equipe (ex: Chrome, Firefox).
    3.  Abra as Ferramentas do Desenvolvedor (geralmente `F12` ou `Ctrl+Shift+I`).
    4.  Vá para a aba "Network" (Rede).
    5.  Acesse uma URL do seu servidor (ex: `http://localhost:8080/`).
    6.  Clique na requisição para a página principal.
    7.  Inspecione as seções "Request Headers" e "Response Headers".
*   **Resultado Esperado:**
    *   **Request Headers:** Deve conter `Host: localhost:8080`, `User-Agent`, `Accept`, etc.
    *   **Response Headers:** Deve conter `HTTP/1.1 200 OK`, `Content-Type: text/html`, `Content-Length`, `Date`, `Server: webserv`, etc.

### 3.2. Compatibilidade para servir um site estático completo

#### Teste 3.2.1: Carregamento de site estático
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Acesse `http://localhost:8080/` no navegador.
    3.  Verifique se a página carrega completamente, incluindo estilos (CSS) e scripts (JS), se houver.
    4.  Inspecione a aba "Network" para garantir que todos os recursos foram carregados com `200 OK`.
*   **Resultado Esperado:**
    *   A página deve ser renderizada corretamente, sem erros no console do navegador.
    *   Todos os recursos (HTML, CSS, JS, imagens) devem ser carregados com sucesso (`200 OK`).

### 3.3. Testar uma URL errada no servidor

#### Teste 3.3.1: URL inexistente (404)
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  No navegador, acesse uma URL que não existe (ex: `http://localhost:8080/pagina_inexistente.html`).
*   **Resultado Esperado:**
    *   O navegador deve exibir a página de erro `404 Not Found` do seu servidor.

### 3.4. Testar listagem de diretório (se implementado e permitido)

#### Teste 3.4.1: Listagem de diretório com autoindex
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Certifique-se de que `webserv.conf` tenha uma `location` configurada com `autoindex on;` (ex: `location /uploads/ { autoindex on; root ./www; }`).
    3.  Acesse `http://localhost:8080/uploads/` no navegador.
*   **Resultado Esperado:**
    *   Uma página HTML listando os arquivos e subdiretórios dentro de `www/uploads/`.

### 3.5. Testar uma URL redirecionada

#### Teste 3.5.1: Redirecionamento 3xx
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Certifique-se de que `webserv.conf` tenha uma `location` configurada com um redirecionamento (ex: `location /antigo { redirect /novo; }`).
    3.  No navegador, acesse `http://localhost:8080/antigo`.
*   **Resultado Esperado:**
    *   O navegador deve ser automaticamente redirecionado para `http://localhost:8080/novo`.
    *   Na aba "Network" das Ferramentas do Desenvolvedor, você verá a requisição para `/antigo` com um status `301 Moved Permanently` (ou outro 3xx) e uma nova requisição para `/novo`.

### 3.6. Testes gerais com navegador
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Use o navegador para explorar todas as funcionalidades do seu servidor, incluindo CGI, upload/download, páginas de erro customizadas, etc.
*   **Resultado Esperado:**
    *   Todas as funcionalidades devem operar conforme o esperado, sem erros no console do navegador ou falhas do servidor.

---

## 4. Problemas de Porta

### 4.1. Configurar múltiplas portas e usar diferentes websites

#### Teste 4.1.1: Portas diferentes
*   **Instruções:**
    1.  Modifique `webserv.conf` para ter servidores em portas diferentes:
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
            root ./www_outro; # Crie este diretório e um index.html dentro dele
            # ...
        }
        ```
    2.  Inicie o servidor: `./webserv webserv.conf`
    3.  Acesse `http://localhost:8080/` e `http://localhost:8081/` (ou `http://outro.com:8081/` se configurado no `hosts` ou `curl --resolve`) no navegador ou com `curl`.
*   **Resultado Esperado:**
    *   Conteúdo diferente para cada porta/hostname.

#### Teste 4.1.2: Mesma porta com Virtual Hosting
*   **Instruções:**
    1.  Certifique-se de que `webserv.conf` tenha `localhost`, `example.com` e `ado` configurados na porta `8080`.
    2.  Certifique-se de que `/etc/hosts` tenha as entradas para `example.com` e `ado` apontando para `127.0.0.1`.
    3.  Inicie o servidor: `./webserv webserv.conf`
    4.  Acesse `http://localhost:8080/`, `http://example.com:8080/` e `http://ado:8080/` no navegador.
*   **Resultado Esperado:**
    *   Conteúdo diferente para cada hostname na mesma porta.

### 4.2. Tentar configurar a mesma porta múltiplas vezes (deve falhar)

#### Teste 4.2.1: Porta duplicada no mesmo bloco `server`
*   **Instruções:**
    1.  Modifique `webserv.conf` para ter:
        ```nginx
        server {
            listen 8080;
            listen 8080; # Porta duplicada
            server_name localhost;
            root ./www;
            # ...
        }
        ```
    2.  Inicie o servidor: `./webserv webserv.conf`
*   **Resultado Esperado:**
    *   O servidor deve falhar ao iniciar, reportando um erro de configuração ou de `bind()` duplicado.

#### Teste 4.2.2: Duas instâncias do servidor na mesma porta
*   **Instruções:**
    1.  Inicie uma instância do servidor: `./webserv webserv.conf` (em um terminal).
    2.  Em outro terminal, tente iniciar outra instância na mesma porta: `./webserv webserv.conf`
*   **Resultado Esperado:**
    *   A segunda instância deve falhar ao iniciar, reportando um erro de `bind()` (endereço já em uso).

### 4.3. Lançar múltiplos servidores com configurações diferentes, mas com portas comuns

#### Teste 4.3.1: Múltiplos servidores virtuais na mesma porta
*   **Instruções:**
    1.  Certifique-se de que `webserv.conf` define múltiplos blocos `server` na mesma porta (ex: `localhost`, `example.com`, `ado` na porta `8080`).
    2.  Inicie o servidor: `./webserv webserv.conf`
    3.  Teste cada hostname via navegador ou `curl --resolve`.
*   **Resultado Esperado:**
    *   Todos os servidores virtuais devem funcionar corretamente.

#### Teste 4.3.2: Configuração inválida em um dos servidores virtuais
*   **Instruções:**
    1.  Modifique um dos blocos `server` em `webserv.conf` para ter uma configuração inválida (ex: `root` para um diretório inexistente, `client_max_body_size` com valor inválido).
    2.  Inicie o servidor: `./webserv webserv.conf`
    3.  Tente acessar o servidor virtual com a configuração inválida.
*   **Resultado Esperado:**
    *   O servidor deve iniciar.
    *   Ao tentar acessar o servidor virtual com a configuração inválida, ele deve retornar um erro apropriado (ex: `500 Internal Server Error`, `404 Not Found` se o root for inválido), sem afetar os outros servidores virtuais.

---

## 5. Siege & Teste de Estresse

### 5.1. Usar Siege para rodar testes de estresse

#### Teste 5.1.1: GET simples em página vazia
*   **Instruções:**
    1.  Certifique-se de que o servidor `webserv` esteja rodando.
    2.  Certifique-se de que seu `webserv.conf` tenha uma rota para uma página HTML simples e leve (ex: `index.html` em `www/`).
    3.  Em outro terminal, execute:
        ```bash
        siege -c 25 -r 100 -b http://localhost:8080/
        ```
        (onde `-c 25`: 25 usuários concorrentes, `-r 100`: 100 repetições, `-b`: benchmark, sem atrasos).
*   **Resultado Esperado:**
    *   A ferramenta `siege` deve reportar um alto número de transações.
    *   Baixa taxa de falhas.
    *   Um tempo médio de resposta aceitável.

### 5.2. Disponibilidade acima de 99.5% para um GET simples em página vazia com `siege -b`

#### Teste 5.2.1: Verificação de Disponibilidade
*   **Instruções:**
    1.  Execute o `siege` conforme o Teste 5.1.1.
*   **Resultado Esperado:**
    *   Na saída do `siege`, a métrica "Availability" (Disponibilidade) deve ser `>= 99.50 %`.

### 5.3. Verificar se não há vazamento de memória

#### Teste 5.3.1: Monitoramento de uso de memória
*   **Instruções:**
    1.  Inicie o servidor: `./webserv webserv.conf`
    2.  Em outro terminal, monitore o uso de memória do processo `webserv` (PID).
        *   No Linux/macOS: `top` ou `htop` (observe a coluna `RSS` ou `MEM`).
        *   Ou use: `ps aux | grep webserv` e anote o PID, depois `watch -n 1 "ps -p <PID_DO_WEBSERV> -o rss,vsz"`
    3.  Execute um teste de estresse prolongado:
        ```bash
        siege -b -t 5M http://localhost:8080/
        ```
        (5 minutos de teste).
    4.  Continue monitorando o uso de memória durante e após o teste.
*   **Resultado Esperado:**
    *   O uso de memória do processo `webserv` deve se estabilizar após um pico inicial.
    *   Não deve haver um crescimento constante e indefinido do uso de memória.

### 5.4. Verificar se não há conexões penduradas (hanging connections)

#### Teste 5.4.1: Monitoramento de conexões
*   **Instruções:**
    1.  Inicie o servidor: `./webserv webserv.conf`
    2.  Em outro terminal, monitore as conexões abertas pelo processo `webserv` (PID).
        ```bash
        lsof -i :8080 | grep webserv
        # ou
        netstat -an | grep 8080
        ```
    3.  Execute um teste de estresse com `siege` (ex: `siege -c 50 -r 100 http://localhost:8080/`).
    4.  Observe o número de conexões nos estados `ESTABLISHED` e `TIME_WAIT`.
*   **Resultado Esperado:**
    *   O número de conexões `ESTABLISHED` deve aumentar durante o teste e diminuir após o término.
    *   O número de conexões `TIME_WAIT` pode aumentar, mas deve eventualmente diminuir.
    *   Não deve haver um acúmulo excessivo de conexões `ESTABLISHED` que não são fechadas.

### 5.5. Usar `siege` indefinidamente sem ter que reiniciar o servidor

#### Teste 5.5.1: Operação contínua sob carga
*   **Instruções:**
    1.  Inicie o servidor: `./webserv webserv.conf`
    2.  Em outro terminal, execute o `siege` com uma duração longa:
        ```bash
        siege -b -t 30M http://localhost:8080/
        ```
        (30 minutos de teste, ou mais).
*   **Resultado Esperado:**
    *   O servidor `webserv` deve continuar respondendo às requisições do `siege` durante todo o período do teste sem travar ou precisar ser reiniciado.
    *   A disponibilidade deve permanecer alta.
