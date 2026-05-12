HEADERS=ascii-renderer-nc.h
CC=gcc -Wall -g

all: tuascii

tuascii: ascii-renderer-nc.o
	$(CC) $^ -o $@ -lm -lncurses -lcurl

install: tuascii
	cp tuascii /usr/local/bin/

%.o: %.c $(HEADERS)
	$(CC) $< -c -o $@

clean:
	rm -f tuascii *.o
