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

-   **Funcionalidade HTTP (Parcial)**:
    -   **Método GET**: O servidor consegue servir um site estático (HTML, CSS, etc.) de forma funcional.
    -   **CGI (Common Gateway Interface)**:
        -   O servidor executa scripts CGI (Python) para requisições `GET` e `POST`.
        -   Para `GET`, a `QUERY_STRING` é passada corretamente via variáveis de ambiente.
        -   Para `POST`, o corpo da requisição é passado para o `stdin` do script CGI, e o servidor fecha o pipe para sinalizar `EOF`.

## 🚧 Funcionalidades Mandatórias Pendentes ou em Progresso

-   **Método `DELETE`**:
    -   **Status**: Não iniciado.
    -   **Exigência**: O servidor precisa aceitar e processar requisições `DELETE` para remover um recurso.

-   **Upload de Arquivos**:
    -   **Status**: ✅ Concluído.
    -   **Exigência**: "Os clientes devem ser capazes de enviar arquivos."
    -   **Detalhes**: A lógica para analisar `multipart/form-data` foi implementada e depurada, permitindo o upload e salvamento de arquivos no diretório `www/uploads` com sucesso.

-   **Arquivo de Configuração Avançado**:
    -   **Status**: Apenas o básico foi implementado.
    -   **Exigência**: O arquivo de configuração deve permitir uma gestão muito mais detalhada. Faltam:
        -   **Múltiplas Portas**: "Definir todos os pares interface:porta nos quais seu servidor irá ouvir".
        -   **`client_max_body_size`**: "Definir o tamanho máximo permitido para os corpos de solicitação do cliente."
        -   **Páginas de Erro Customizadas**: "Configurar páginas de erro padrão."
        -   **Regras por Rota (`location`)**:
            -   Limitar métodos HTTP aceitos (ex: `GET`, `POST`).
            -   Configurar redirecionamento HTTP.
            -   Ativar/desativar listagem de diretório.
            -   Definir o caminho para onde os uploads devem ser salvos.

-   **Robustez e Resiliência**:
    -   **Status**: Em progresso (melhorado).
    -   **Exigência**: "Seu servidor deve permanecer disponível em todos os momentos." / "Seu programa não deve travar sob nenhuma circunstância."
    -   **Detalhes**: O servidor não trava mais na tentativa de upload, mas a robustez geral ainda precisa ser aprimorada para outros cenários de erro e para lidar com requisições `chunked`.

-   **Requisições em Partes (`Transfer-Encoding: chunked`)**:
    -   **Status**: Não iniciado.
    -   **Exigência**: "Para solicitações em partes, seu servidor precisa desagrupá-las".
    -   **Detalhes**: O servidor atualmente depende do `Content-Length` e não está preparado para analisar requisições "chunked".

---

**Recomendação**: O próximo passo crucial é **depurar e corrigir a funcionalidade de upload de arquivos**, pois é um requisito mandatório que atualmente está quebrando o servidor. Depois disso, a implementação do método `DELETE` seria o próximo passo lógico.
