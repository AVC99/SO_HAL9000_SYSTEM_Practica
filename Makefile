CC = gcc
CFLAGS = -Wall -Wextra -g 
FFLAGS = -lpthread 

all: Bowman Poole Discovery

network_utils.o: network_utils.c 
io_utils.o: io_utils.c 
bowman_thread_handler.o: bowman_thread_handler.c bowman_thread_handler.h io_utils.h network_utils.h
bowman_utilities.o: bowman_utilities.c bowman_utilities.h io_utils.h network_utils.h
Bowman: Bowman.c bowman_utilities.o io_utils.o network_utils.o
Poole: Poole.c io_utils.o network_utils.o
Discovery: Discovery.c io_utils.o network_utils.o

%: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(FFLAGS)

clean:
	rm -vf *.o

clean_all: clean
	rm -vf Bowman Poole Discovery *.txt