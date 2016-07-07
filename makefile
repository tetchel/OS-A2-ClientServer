CFLAGS+=-g -Wall
LDFLAGS+=-pthread

all:client server

client:client.c
	$(CC) client.c $(CFLAGS) -o $@ $(LDFLAGS)

server:server.c
	$(CC) server.c $(CFLAGS) -o $@ $(LDFLAGS)
