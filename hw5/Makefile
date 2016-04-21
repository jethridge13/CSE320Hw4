CC = gcc
CFLAGS = -Wall -Werror -g
BINS = server client chat

all: $(BINS)

server:
	$(CC) $(CFLAGS) server.c -o server -pthread

client:
	$(CC) $(CFLAGS) client.c -o client

chat: 
	$(CC) $(CFLAGS) chat.c -o chat

clean:
	rm -f *.o $(BINS)
