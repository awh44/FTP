CC=gcc
COMMON_OPTS=-Iinclude/ -o$@ $(DEBUG)
COMMON_DEPENDENCIES=bin/ftp.o bin/string_t.o bin/status_t.o bin/log.o
BIN_OPTS=$(COMMON_OPTS) -c $^
PROG_OPTS=$(COMMON_OPTS) $^

all: ftpserver ftpclient

ftpserver: bin/ftpserver.o $(COMMON_DEPENDENCIES)
	$(CC) $(PROG_OPTS) -lpthread 

ftpclient: bin/ftpclient.o $(COMMON_DEPENDENCIES)
	$(CC) $(PROG_OPTS)

bin/ftpserver.o: src/ftpserver.c
	$(CC) $(BIN_OPTS)

bin/ftpclient.o: src/ftpclient.c
	$(CC) $(BIN_OPTS)

bin/ftp.o: src/ftp.c
	$(CC) $(BIN_OPTS)

bin/string_t.o: src/string_t.c
	$(CC) $(BIN_OPTS)

bin/status_t.o: src/status_t.c
	$(CC) $(BIN_OPTS)

bin/log.o: src/log.c
	$(CC) $(BIN_OPTS)

clean:
	rm -rf bin/* ftpclient ftpserver
