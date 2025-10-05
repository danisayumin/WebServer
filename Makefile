# Nome do executável
NAME = webserv

# Compilador e flags
CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Arquivos fonte (adicione seus arquivos .cpp aqui)
SRCS = main.cpp Server.cpp ClientConnection.cpp ConfigParser.cpp

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

# Declaração de que as regras não são arquivos
.PHONY: all clean fclean re
