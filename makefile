#file: makefile
#Author: Bernhard Ruhm 12019841
#date: 12-11-2021
#makefile for forksort.c
CC = gcc
FLAGS = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g
 
.PHONY: all clean
all: client server

client: client.o
	$(CC) $(FLAGS) -o $@ $^ 

server: server.o
	$(CC) $(FLAGS) -o $@ $^ 

client.o: client.c
	$(CC) $(FLAGS) -c -o $@ $<

server.o: server.c
	$(CC) $(FLAGS) -c -o $@ $<
	
clean:
	rm -rf *.o client server
