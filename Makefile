CC = gcc
CFLAGS = -Wall -Wextra -lpthread 

all: Bowman Poole Discovery

read_until.o: read_until.c 
bowman_utilities.o: bowman_utilities.c bowman_utilities.h read_until.h
Bowman: Bowman.c bowman_utilities.o read_until.o
Poole: Poole.c read_until.o
Discovery: Discovery.c read_until.o

%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o

clean_all: clean
	rm -f Bowman Poole Discovery valgrind-out.txt