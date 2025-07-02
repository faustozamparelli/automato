CC=clang
CFLAGS=-Wall -Wextra

all: rule110.c

rule110: rule110.c
	$(CC) $(CFLAGS) rule110.c -o rule110

clean:
	rm rule110
