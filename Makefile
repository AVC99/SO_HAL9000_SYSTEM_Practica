CC = gcc
CFLAGS = -Wall -Wextra

all: main Bowman Poole

main: main.c
	$(CC) $(CFLAGS) -o main main.c 

Bowman: Bowman.c
	$(CC) $(CFLAGS) -o Bowman Bowman.c

Poole: Poole.c
	$(CC) $(CFLAGS) -o Poole Poole.c

clean: 
	rm -f *.o

clean_all: clean
	rm -f main Bowman Poole valgrind.txt