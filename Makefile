all: tuascii

tuascii: ascii-renderer-nc.o
	gcc $^ -o $@ -lm -lncurses -lcurl

install: tuascii
	cp tuascii /usr/local/bin/

%.o: %.c 
	gcc $< -c -o $@

clean:
	rm -f tuascii *.o
