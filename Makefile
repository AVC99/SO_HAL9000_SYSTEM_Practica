CC = gcc
CFLAGS = -Wall -Wextra -g 
FFLAGS = -lpthread -lrt

all: Bowman Poole Discovery

network_utils.o: network_utils.c 
io_utils.o: io_utils.c 
semaphore_v2.o: semaphore_v2.c semaphore_v2.h
bowman_thread_handler.o: bowman_thread_handler.c bowman_thread_handler.h io_utils.h network_utils.h
poole_thread_handler.o: poole_thread_handler.c poole_thread_handler.h io_utils.h network_utils.h
bowman_utilities.o: bowman_utilities.c bowman_utilities.h io_utils.h network_utils.h semaphore_v2.o
Bowman: Bowman.c bowman_utilities.o io_utils.o network_utils.o bowman_thread_handler.o
Poole: Poole.c io_utils.o network_utils.o poole_thread_handler.o semaphore_v2.o
Discovery: Discovery.c io_utils.o network_utils.o

%: %.c
	$(CC) $(CFLAGS) -o $@ $^ $(FFLAGS)

clean:
	rm -vf *.o

clean_all: clean
	rm -vf Bowman Poole Discovery *.txt