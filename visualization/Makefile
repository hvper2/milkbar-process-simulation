CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I../include

.PHONY: all clean

all: viz

viz: viz.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f viz
