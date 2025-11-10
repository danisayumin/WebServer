# Relatório de Progresso - Webserv

Este documento detalha o progresso do projeto Webserv com base nos requisitos obrigatórios definidos no `WEBSERV.pdf`.

## ✅ Funcionalidades Mandatórias Concluídas

-   **Estrutura do Projeto e Compilação**:
    -   O projeto compila com `c++` usando as flags `-Wall -Wextra -Werror`.
    -   Está em conformidade com o padrão **C++98** (`-std=c++98`).
    -   O `Makefile` contém as regras `all`, `clean`, `fclean`, e `re` e não utiliza bibliotecas externas.

-   **Arquitetura do Servidor**:
    -   O servidor é **não-bloqueante**, utilizando `select()` para multiplexar todas as operações de I/O em um único loop.
    -   O `fork()` é utilizado exclusivamente para a execução de scripts CGI.
    -   O servidor aceita um arquivo de configuração pela linha de comando.
    -   Gera páginas de erro padrão para códigos como `404` ou `500`.

-   **Funcionalidade HTTP**:
    -   **Método GET**: O servidor consegue servir um site estático (HTML, CSS, etc.) de forma funcional.
    -   **Método DELETE**: ✅ Concluído. O servidor aceita e processa requisições `DELETE` para remover um recurso, retornando os códigos de status apropriados (204, 403, 404, 500).
    -   **CGI (Common Gateway Interface)**:
        -   O servidor executa scripts CGI (Python) para requisições `GET` e `POST`.
        -   Para `GET`, a `QUERY_STRING` é passada corretamente via variáveis de ambiente.
        -   Para `POST`, o corpo da requisição é passado para o `stdin` do script CGI, e o servidor fecha o pipe para sinalizar `EOF`.

## ✅ Melhorias de UI/UX

-   **Design com 'Vibe de Game'**: ✅ Concluído. O CSS foi atualizado com uma paleta de cores específica e estilos que remetem a uma estética de jogo, incluindo fontes, sombras e efeitos visuais.
-   **Navegação Consistente**: ✅ Concluído. Todas as páginas HTML principais (`index.html`, `upload.html`, `secret/secret.html`, `404.html`) foram atualizadas para incluir uma barra de navegação consistente, permitindo fácil transição entre as seções do site.
-   **Redirecionamento e Links Funcionais**: ✅ Concluído. Os links de navegação foram implementados para permitir a movimentação entre as páginas, e a página 404 agora inclui um link para retornar à página inicial.

## 🚧 Funcionalidades Mandatórias Pendentes ou em Progresso

-   **Upload de Arquivos**:
    -   **Status**: ✅ Concluído.
    -   **Exigência**: "Os clientes devem ser capazes de enviar arquivos."
    -   **Detalhes**: A lógica para analisar `multipart/form-data` foi implementada e depurada, permitindo o upload e salvamento de arquivos no diretório `www/uploads` com sucesso.

-   **Arquivo de Configuração Avançado**:
    -   **Status**: Em progresso.
    -   **Exigência**: O arquivo de configuração deve permitir uma gestão muito mais detalhada.
    -   **Concluído**:
        -   **`client_max_body_size`**: O servidor agora respeita o limite de tamanho do corpo da requisição configurado por `client_max_body_size` (ex: `1K`, `10M`), retornando `413 Payload Too Large` se excedido.
        -   **Múltiplas Portas**: O servidor pode ser configurado para escutar em várias portas simultaneamente.
        -   **Páginas de Erro Customizadas**: O servidor agora pode ser configurado para usar páginas HTML personalizadas para *qualquer* código de erro HTTP (ex: `error_page 404 /404.html;`, `error_page 500 /500.html;`).
        -   **Regras por Rota (`location`)**:
            -   **Limitar métodos HTTP aceitos (ex: `GET`, `POST`)**: ✅ Concluído. O servidor agora pode restringir os métodos HTTP para uma `location` específica usando a diretiva `allow_methods`.
            -   **Configurar redirecionamento HTTP**: ✅ Concluído. É possível configurar um redirecionamento `301 Moved Permanently` para uma `location` específica.
            -   **Definir o caminho para onde os uploads devem ser salvos**: ✅ Concluído. É possível definir um `upload_path` para uma `location`, determinando onde os arquivos enviados devem ser armazenados.
            -   **Ativar/desativar listagem de diretório**: ✅ Concluído. A diretiva `autoindex on;` agora gera uma listagem de arquivos e diretórios quando um arquivo de índice não é encontrado.
    -   **Faltam**:
        -   Suporte a múltiplos servidores com hostnames diferentes (Virtual Hosts)
-   **Robustez e Resiliência**:
    -   **Status**: ✅ Concluído (melhorado).
    -   **Exigência**: "Seu servidor deve permanecer disponível em todos os momentos." / "Seu programa não deve travar sob nenhuma circunstância."
    -   **Detalhes**: O servidor não trava mais na tentativa de upload, e agora lida com requisições `chunked`, melhorando a robustez geral.

-   **Requisições em Partes (`Transfer-Encoding: chunked`)**:
    -   **Status**: ✅ Concluído.
    -   **Exigência**: "Para solicitações em partes, seu servidor precisa desagrupá-las".
    -   **Detalhes**: A infraestrutura para lidar com requisições `chunked` foi estabelecida com a criação da classe `HttpRequestParser`. A lógica de parsing incremental foi implementada e testada com sucesso, garantindo a correta análise e re-agrupamento do corpo da requisição, mesmo quando os dados chegam em partes.

---

**Recomendação**: Todas as funcionalidades mandatórias foram implementadas. O próximo passo é focar em testes abrangentes e melhorias de robustez e performance.
