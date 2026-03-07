# Program names
NAME        = webserv
TEST_CLIENT = test_client

# Compiler and flags
CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98

# Server Source files
SRCS        = main.cpp \
              ConfigNode.cpp \
              Lexer.cpp \
              Parser.cpp \
              ConfigValidator.cpp \
              ConfigLoader.cpp \
              Utils.cpp \
              Server.cpp \
              Request.cpp \
              Response.cpp

# Object files
OBJS        = $(SRCS:.cpp=.o)

# Default rule
all: $(NAME)

# Compile the main webserv program
$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "✅ $(NAME) successfully compiled!"

# Compile .cpp to .o
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Rule to compile your friend's test client (ser.cpp)
client: ser.cpp
	$(CXX) $(CXXFLAGS) ser.cpp -o $(TEST_CLIENT)
	@echo "✅ $(TEST_CLIENT) successfully compiled!"

# Clean object files
clean:
	rm -f $(OBJS)
	@echo "🧹 Object files cleaned."

# Clean objects and executables
fclean: clean
	rm -f $(NAME) $(TEST_CLIENT)
	@echo "🗑️  Executables removed."

# Recompile everything
re: fclean all

.PHONY: all clean fclean re client
