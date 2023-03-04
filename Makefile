CC=gcc
CLFAGS= -g -std=gnu99 -Ilib
DEP=open62541.h open62541.c
OBJ=open62541.o

open62541.o: open62541.c open62541.h
	$(CC) $(CFLAGS) -c open62541.c -o open62541.o

build: $(OBJ) $(DEP)
	$(CC) $(CFLAGS) $(OBJ) open62541-subscriber.c -o open62541-subscriber
	$(CC) $(CFLAGS) $(OBJ) my-publisher.c -o my-publisher

clean: 
	$(RM) open62541-subscriber my-publisher
	$(RM) my-publisher
