#!/bin/bash
CFLAGS=-std=gnu99 -Wall -g -rdynamic
LIB	=-lpthread
all:node coordinator client
node:node.o common.o
	gcc -o node node.o common.o $(CFLAGS) $(LIB)
coordinator:coordinator.o common.o jerasure.o reed_sol.o cauchy.o liberation.o timing.o galois.o
	gcc -o coordinator coordinator.o common.o jerasure.o reed_sol.o cauchy.o liberation.o timing.o galois.o $(CFLAGS) $(LIB) -L/usr/local/lib -lgf_complete
coordinator.o:coordinator.c common.h jerasure.h reed_sol.h cauchy.h liberation.h timing.h
	gcc -c coordinator.c $(CFLAGS) $(LIB) -L/usr/local/lib -lgf_complete
galois.o:galois.c galois.h
	gcc -c galois.c $(CFLAGS) $(LIB)
jerasure.o:jerasure.c jerasure.h galois.h
	gcc -c jerasure.c $(CFLAGS) $(LIB)
reed_sol.o:reed_sol.c reed_sol.h jerasure.h galois.h
	gcc -c reed_sol.c $(CFLAGS) $(LIB) -L/usr/local/lib -lgf_complete
cauchy.o:cauchy.c cauchy.h galois.h jerasure.h
	gcc -c cauchy.c $(CFLAGS) $(LIB)
liberation.o:liberation.c liberation.h galois.h jerasure.h
	gcc -c liberation.c $(CFLAGS) $(LIB)
timing.o:timing.c timing.h
	gcc -c timing.c $(CFLAGS) $(LIB)
client:client.o common.o
	gcc -o client client.o common.o $(CFLAGS) $(LIB)
node.o:node.c common.h
	gcc -c node.c $(CFLAGS) $(LIB)
common.o:common.c common.h
	gcc -c common.c $(CFLAGS) $(LIB)
client.o:client.c common.h
	gcc -c client.c $(CFLAGS) $(LIB)
.PHNOY:clean
clean:
	-rm -rf *.o
	-rm -rf client
	-rm -rf node
	-rm -rf coordinator