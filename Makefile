CC = gcc
CFLAGS = -Wall -Wextra -lpthread 

all: Bowman Poole 

read_until.o: read_until.c read_until.h
	$(CC) -c read_until.c

bowman_utilities.o: bowman_utilities.c bowman_utilities.h read_until.o
	$(CC) -c bowman_utilities.c read_until.o

Bowman: Bowman.c bowman_utilities.o
	$(CC) $(CFLAGS) -o Bowman Bowman.c bowman_utilities.o read_until.o

Poole: Poole.c read_until.o
	$(CC) $(CFLAGS) -o Poole Poole.c read_until.o

clean: 
	rm -f *.o

clean_all: clean
	rm -f main Bowman Poole valgrind-out.txt