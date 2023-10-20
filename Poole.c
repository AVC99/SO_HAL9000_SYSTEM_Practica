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

typedef struct
{
  char *servername;
  char *folder;
  char *firstIP;
  int firstPort;
  char *secondIP;
  int secondPort;
} Poole;

// Read until the delimiter
char *readUntil(char del, int fd)
{

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

// TODO: Check if the file is correctly formatted
Poole savePoole(int fd)
{
  Poole poole;

  poole.servername = readUntil('\n', fd);
  poole.folder = readUntil('\n', fd);
  poole.firstIP = readUntil('\n', fd);
  char *port = readUntil('\n', fd);
  poole.firstPort = atoi(port);
  free(port);
  poole.secondIP = readUntil('\n', fd);
  port = readUntil('\n', fd);
  poole.secondPort = atoi(port);
  free(port);

  return poole;
}

void phaseOneTesting(Poole poole)
{
  char *buffer;
  write(1, "File read correctly:\n", strlen("File read correctly:\n"));
  asprintf(&buffer, "Server - %s\n", poole.servername);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Folder - %s\n", poole.folder);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "First IP - %s\n", poole.firstIP);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "First Port - %d\n", poole.firstPort);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Second IP - %s\n", poole.secondIP);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Second Port - %d\n", poole.secondPort);
  write(1, buffer, strlen(buffer));
  write(1, "\n", strlen("\n"));
  free(buffer);
}

int main(int argc, char *argv[])
{

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

  Poole poole = savePoole(fd);

  // THIS IS FOR PHASE 1 TESTING
  phaseOneTesting(poole);

  free(poole.servername);
  free(poole.folder);
  free(poole.firstIP);
  free(poole.secondIP);

  close(fd);

  return 0;
}