CC=gcc
CFLAGS=-Wall
DBFLAGS=-g -fsanitize=address -fsanitize=undefined -fsanitize=leak -Wextra

all: shell.c
	$(CC) $(CFLAGS) -o cs5374_sh shell.c
debug: shell.c
	$(CC) $(CFLAGS) $(DBFLAGS) -DDEBUG -o cs5374_sh shell.c
clean:
	rm -f cs5374_sh