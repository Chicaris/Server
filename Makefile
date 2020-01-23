CC=gcc
CFLAGS=-Wall
ifeq ($(DEBUG), 1)
	CFLAGS+= -g -DDEBUG
endif

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

all: server client

server: server.o server.h
	$(CC) $(CFLAGS) -o $@ $<
	
client: client.o server.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f *.o
	rm -f client server
	rm -f project.zip

zip: server.c server.h client.c Makefile
	zip -l project.zip ./*.c ./*.h ./Makefile
