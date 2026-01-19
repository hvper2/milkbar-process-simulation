CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g -Iinclude
LDFLAGS = -lpthread

# Programy do zbudowania
PROGRAMS = bar kasjer obsluga klient kierownik

.PHONY: all clean run

all: $(addprefix bin/, $(PROGRAMS)) | logs

# Tworzenie katalogów
bin logs obj:
	mkdir -p $@

# Kompilacja obiektów
obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c -o $@ $<

# Linkowanie programów
bin/klient: obj/klient.o obj/utils.o | bin
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

bin/%: obj/%.o obj/utils.o | bin
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf obj bin logs/*.log

run: all
	./bin/bar
