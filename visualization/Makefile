CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
INCLUDES = -I../include
TARGET = viz
SRC = viz.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean
