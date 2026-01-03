CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -I$(INCDIR)
LDFLAGS =

# Katalogi
SRCDIR = src
INCDIR = include
OBJDIR = obj
LOGDIR = logs
BINDIR = bin

# Pliki źródłowe
SOURCES = bar.c kasjer.c obsluga.c klient.c kierownik.c utils.c
OBJECTS = $(SOURCES:%.c=$(OBJDIR)/%.o)
BINARY = $(BINDIR)/bar
EXECUTABLES = $(BINDIR)/bar $(BINDIR)/kasjer $(BINDIR)/obsluga $(BINDIR)/klient $(BINDIR)/kierownik

.PHONY: all clean run test

all: $(BINARY) $(BINDIR)/kasjer $(BINDIR)/obsluga $(BINDIR)/klient $(BINDIR)/kierownik

# Katalogi
$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

$(LOGDIR):
	mkdir -p $(LOGDIR)

# Główny program
$(BINARY): $(OBJDIR)/bar.o $(OBJDIR)/utils.o | $(BINDIR) $(LOGDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Procesy
$(BINDIR)/kasjer: $(OBJDIR)/kasjer.o $(OBJDIR)/utils.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/obsluga: $(OBJDIR)/obsluga.o $(OBJDIR)/utils.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/klient: $(OBJDIR)/klient.o $(OBJDIR)/utils.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BINDIR)/kierownik: $(OBJDIR)/kierownik.o $(OBJDIR)/utils.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Obiekty
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(BINDIR) $(LOGDIR)/*.log

run: $(BINARY)
	./$(BINARY)


