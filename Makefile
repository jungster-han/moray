CC     = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
SRC    = src/lexer.c src/parser.c src/interpreter.c src/value.c src/env.c src/ast.c src/main.c
OUT    = moray

all:
	$(CC) $(CFLAGS) $(SRC) -o $(OUT)

run: all
	./$(OUT) $(FILE)

clean:
	rm -f $(OUT)

.PHONY: all run clean
