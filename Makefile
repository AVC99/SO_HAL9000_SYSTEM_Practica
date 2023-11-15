CC = gcc
CFLAGS = -Wall -Wextra -lpthread 

all: Bowman Poole Discovery

io_utils.o: io_utils.c 
bowman_utilities.o: bowman_utilities.c bowman_utilities.h io_utils.h
Bowman: Bowman.c bowman_utilities.o io_utils.o
Poole: Poole.c io_utils.o
Discovery: Discovery.c io_utils.o

%: %.c
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -vf *.o

clean_all: clean
	rm -vf Bowman Poole Discovery valgrind-out.txt