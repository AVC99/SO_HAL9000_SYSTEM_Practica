#define _GNU_SOURCE
#include "read_until.h"
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>


// arnau.vives joan.medina I3_6

typedef struct
{
  char *firstIP;
  int firstPort;
  char *secondIP;
  int secondPort;
} Discovery;

Discovery discovery;

/**
 * Saves the discovery information from the file
*/
void saveDiscovery(int fd)
{
  write(1, "Reading configuration file...\n", strlen("Reading configuration file...\n"));
  discovery.firstIP = readUntil('\n', fd);
  printf("First IP: %s\n", discovery.firstIP);
  char *firstPort = readUntil('\n', fd);
  discovery.firstPort = atoi(firstPort);
  free(firstPort);
  discovery.secondIP = readUntil('\n', fd);
  char *secondPort = readUntil('\n', fd);
  discovery.secondPort = atoi(secondPort);
  free(secondPort);
  
  close(fd);
}
/**
 * Frees all the memory allocated from the global Discovery
*/
void freeMemory()
{
  free(discovery.firstIP);
  free(discovery.secondIP);
}

/**
 * Closes the program
*/
void closeProgram()
{
  freeMemory();
  exit(0);
}

void runDiscovery()
{
  


}

int main(int argc, char *argv[])
{
  //Reprogram the SIGINT signal
  signal(SIGINT, closeProgram);

  if (argc != 2)
  {
    write(1, "Error: Invalid number of arguments\n", strlen("Error: Invalid number of arguments\n"));
    return 1;
  }
  int fd = open (argv[1], O_RDONLY);
  if (fd < 0)
  {
    write(1, "Error: File not found\n", strlen("Error: File not found\n"));
    return 1;
  }

  saveDiscovery(fd);

  runDiscovery();

  freeMemory();
  return 0;
}