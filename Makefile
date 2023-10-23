CC = gcc
CFLAGS = -Wall -Wextra -lpthread 

all: Bowman Poole

bowman_utilities.o: bowman_utilities.c bowman_utilities.h
	$(CC) -c bowman_utilities.c

Bowman: Bowman.c bowman_utilities.o
	$(CC) $(CFLAGS) -o Bowman Bowman.c bowman_utilities.o

Poole: Poole.c
	$(CC) $(CFLAGS) -o Poole Poole.c

clean: 
	rm -f *.o

clean_all: clean
	rm -f main Bowman Poole valgrind-out.txt