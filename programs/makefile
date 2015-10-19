CC=gcc
OPTS=-Iinclude/ -c -o$@ $?

ftpclient: bin/ftpclient.o bin/string_t.o
	$(CC) -o$@ bin/*

bin/ftpclient.o: src/ftpclient.c
	$(CC) $(OPTS)

bin/string_t.o: src/string_t.c
	$(CC) $(OPTS)

clean:
	rm -rf bin/* ftpclient
