CC = gcc
CFLAGS = -Wall -Wextra -lpthread 

all: Bowman Poole

connect.o: connect.c connect.h
	$(CC) -c connect.c

Bowman: Bowman.c connect.o
	$(CC) $(CFLAGS) -o Bowman Bowman.c connect.o

Poole: Poole.c
	$(CC) $(CFLAGS) -o Poole Poole.c

clean: 
	rm -f *.o

clean_all: clean
	rm -f main Bowman Poole valgrind-out.txt