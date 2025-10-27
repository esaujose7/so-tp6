CC=gcc
CFLAGS=-Wall -Werror -std=c99 -pthread

all: toupperd

toupperd: toupperd.o
	$(CC) $(CFLAGS) -o toupperd toupperd.o

toupperd.o: toupperd.c
	$(CC) $(CFLAGS) -c toupperd.c

clean:
	rm -f toupperd toupperd.o
