CC?=gcc -Wall

all: tuascii

tuascii: ascii-renderer-nc.o
	$(CC) $^ -o $@ -lm -lncurses -lcurl

install: tuascii
	cp tuascii /usr/local/bin/

%.o: %.c 
	$(CC) $< -c -o $@

clean:
	rm -f tuascii *.o
