# add -g to CC for debugging
CC = gcc
LFLAGS = -pthread

all:	crawl

crawl:	fifoq.h crawl.o fifoq.o
	$(CC) -o crawl crawl.o fifoq.o $(LFLAGS)
