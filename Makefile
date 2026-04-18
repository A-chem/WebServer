NAME    = webserv

CC      = c++
CFLAGS  = -Wall -Wextra -std=c++98 -I inc

SRCS    = src/main.cpp \
          src/config/Lexer.cpp \
          src/config/Parser.cpp \
          src/config/ConfigNode.cpp \
          src/config/ConfigValidator.cpp \
          src/config/ConfigLoader.cpp \
          src/config/CGIConfig.cpp \
          src/utils/Utils.cpp \
          src/server/Client.cpp \
          src/server/Socket.cpp \
          src/server/Epoll.cpp \
          src/server/Server.cpp \
          src/server/Request.cpp \
          src/server/ErrorPages.cpp \
          src/server/Response.cpp \
          src/server/CGI.cpp

OBJS    = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
