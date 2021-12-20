#file: makefile
#Author: Bernhard Ruhm 12019841
#date: 12-11-2021
#makefile for forksort.c
CC = gcc
FLAGS = -std=c99 -pedantic -Wall -D_DEFAULT_SOURCE -D_BSD_SOURCE -D_SVID_SOURCE -D_POSIX_C_SOURCE=200809L -g
 
OBJECTS = client.o
.PHONY: all clean
all: client

client: $(OBJECTS)
        $(CC) $(FLAGS) -o $@ $^ 
client.o: client.c
         $(CC) $(FLAGS) -c -o $@ $<
clean:
	rm -rf *.o client
