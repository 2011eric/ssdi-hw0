CC=gcc
CFLAGS=-Wall -Wextra
DBFLAGS=-g -fsanitize=address -fsanitize=undefined -fsanitize=leak

all: shell.c
	$(CC) $(CFLAGS) -o shell shell.c
debug: shell.c
	$(CC) $(CFLAGS) $(DBFLAGS) -DDEBUG -o shell shell.c
test_trim: test_trim.c
	$(CC) $(CFLAGS) $(DBFLAGS) -o test_trim test_trim.c