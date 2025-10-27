CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LDFLAGS = -lncurses -lpthread
TARGET = pelutiu

all: $(TARGET)

$(TARGET): pelutiu.c
	$(CC) $(CFLAGS) pelutiu.c -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
