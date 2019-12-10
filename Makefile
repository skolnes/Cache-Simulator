# Makefile for COMP280 Project 5
 
CC = gcc
CFLAGS = -g -Wall -Werror -std=c11 -D_XOPEN_SOURCE=700

all: csim

csim: csim.c cachelab.c cachelab.h
	$(CC) $(CFLAGS) -o csim csim.c cachelab.c -lm 

#
# Clean the src dirctory
#
clean:
	rm -rf *.o
	rm -f csim
	rm -f trace.all trace.f*
	rm -f .csim_results .marker
