CC = gcc
CFLAGS = -Wall -O2
ARGS = hexdumb.c

all: hexdumb hexbuff

# 1. Clean compilation targets (No automatic execution here)
hexdumb: hexdumb.c
	$(CC) $(CFLAGS) -o hexdumb hexdumb.c

hexbuff: hexdumb_buffered.c
	$(CC) $(CFLAGS) -o hexbuff hexdumb_buffered.c

run_dumb: hexdumb
	./hexdumb $(ARGS)

run_buff: hexbuff
	./hexbuff $(ARGS)

run: run_dumb run_buff

clean:
	rm -f hexdumb hexbuff *.o

.PHONY: all run run_dumb run_buff clean

