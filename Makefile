# Nome do executável
NAME = webserv

# Compilador e flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Arquivos fonte (adicione seus arquivos .cpp aqui)
SRCS = main.cpp Server.cpp ClientConnection.cpp ConfigParser.cpp HttpRequest.cpp HttpResponse.cpp HttpRequestParser.cpp

# Arquivos objeto
OBJS = $(SRCS:.cpp=.o)

# Regra padrão: compila tudo
all: $(NAME)

# Regra para criar o executável
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(NAME) $(OBJS)

# Regra para compilar arquivos .cpp em .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Regra para limpar arquivos objeto
clean:
	rm -f $(OBJS)

# Regra para limpar tudo (objetos e executável)
fclean: clean
	rm -f $(NAME)

# Regra para recompilar
re: fclean all

# Inicia a sessão telnet e espera comandos manuais
telnet1:
	@echo "Conectando ao localhost:8080. Digite sua requisição (ex: GET / HTTP/1.1) e pressione ENTER duas vezes."
	telnet localhost 8080

# Envia uma requisição HTTP não padrão ("GARBAGE") automaticamente
telnet2:
	@echo "Enviando requisição 'GARBAGE / HTTP/1.1' e esperando resposta..."
	printf "GARBAGE / HTTP/1.1\r\nHost: localhost\r\n\r\n" | telnet localhost 8080

# Declaração de que as regras não são arquivos
.PHONY: all clean fclean re telnet1 telnet2
