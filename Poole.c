#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>

typedef struct {
  char *servername;
  char *folder;
  char *firstIP;
  int firstPort;
  char *secondIP;
  int secondPort;
} Poole;

// Read until the delimiter
char *readUntil(char del, int fd){

  char *chain = malloc(sizeof(char));
  char c;
  int i = 0, n;

  n = read(fd, &c, 1);

  while (c != del && n != 0)
  {
    chain[i] = c;
    i++;
    chain = realloc(chain, (sizeof(char) * (i + 1)));
    n = read(fd, &c, 1);
  }

  chain[i] = '\0';

  return chain;
}

//TODO: Check if the file is correctly formatted
Poole *savePoole(int fd){
  Poole *poole = malloc(sizeof(Poole));

  poole->servername = readUntil('\n', fd);
  poole->folder = readUntil('\n', fd);
  poole->firstIP = readUntil('\n', fd);
  poole->firstPort = atoi(readUntil('\n', fd));
  poole->secondIP = readUntil('\n', fd);
  poole->secondPort = atoi(readUntil('\n', fd));

  return poole;
}

int main(int argc, char *argv[]){

  Poole *poole = malloc(sizeof(Poole));

  // Check if the arguments are provided
  if (argc < 2)
  {
    write(2, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
    return 1;
  }

  int fd = open(argv[1], O_RDONLY);

  if (fd < 0)
  {
    write(2, "Error: File not found\n", strlen("Error: File not found\n"));
    return 1;
  }

  poole = savePoole(fd);

  free(poole);

  return 0;
}