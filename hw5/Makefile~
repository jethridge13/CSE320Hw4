CC = gcc
CFLAGS = -Wall -Werror
BINS = server client chat

all: $(BINS)

server:
	$(CC) $(CFLAGS) server.c -o server

client:
	$(CC) $(CFLAGS) client.c -o client

chat: 
	$(CC) $(CFLAGS) chat.c -o chat

clean:
	rm -f *.o $(BINS)