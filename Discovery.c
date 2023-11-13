#define _GNU_SOURCE
#include "io_utils.h"
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
int listenFD;

/**
 * Saves the discovery information from the file
 */
void getDiscoveryFromFile(int fd)
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
  int clientFD;
  struct sockaddr_in server;

  if ((listenFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    write(1, "Error creating the socket\n", strlen("Error creating the socket\n"));
    exit(1);
  }

  bzero(&server, sizeof(server));
  server.sin_port = htons(discovery.firstPort);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_pton(AF_INET, discovery.firstIP, &server.sin_addr);

  if (inet_pton(AF_INET, discovery.firstIP, &server.sin_addr))
  {
    write(1, "Error converting the IP address\n", strlen("Error converting the IP address\n"));
    exit(1);
  }
  write(1, "Socket created\n", strlen("Socket created\n"));

  if (bind(listenFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    write(1, "Error binding the socket\n", strlen("Error binding the socket\n"));
    exit(1);
  }

  write(1, "Socket binded\n", strlen("Socket binded\n"));

  if (listen(listenFD, 10) < 0)
  {
    printF("Error while listening\n");
  }

  while (1)
  {
    printF("Waiting for connections...\n");

    clientFD = accept(listenFD, (struct sockaddr *)NULL, NULL);

    printF("\nNew client connected !\n");

    close(clientFD);
  }
}

int main(int argc, char *argv[])
{
  // Reprogram the SIGINT signal
  signal(SIGINT, closeProgram);

  if (argc != 2)
  {
    write(1, "Error: Invalid number of arguments\n", strlen("Error: Invalid number of arguments\n"));
    return 1;
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0)
  {
    write(1, "Error: File not found\n", strlen("Error: File not found\n"));
    return 1;
  }

  getDiscoveryFromFile(fd);

  runDiscovery();

  freeMemory();

  return 0;
}