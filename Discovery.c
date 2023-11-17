#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>

// arnau.vives joan.medina I3_6

Discovery discovery;
int listenFD;

/**
 * Saves the discovery information from the file
 */
void getDiscoveryFromFile(int fd)
{
  printToConsole("Reading configuration file...\n");
  discovery.firstIP = readUntil('\n', fd);
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

  close(listenFD);
}

/**
 * Closes the program
 */
void closeProgram()
{
  freeMemory();
  exit(0);
}

void getSocketData(int clientFD)
{
  char *buffer = malloc(sizeof(char));
  ssize_t bytesread = read(clientFD, buffer, sizeof(buffer));

  for (ssize_t i = 0; i < bytesread; i++)
  {
    printf("0x%02x ", (unsigned char)buffer[i]);
  }
  printf("\n");
  uint16_t headerLength;
  bytesread = read(clientFD, &headerLength, sizeof(headerLength));

  headerLength = ntohs(headerLength); // Convert to host byte order

  printf("Header length: %u\n", headerLength);

  char *header = malloc(headerLength + 1);
  read(clientFD, header, headerLength);
  printf("Header: %s\n", header);

  char *data = malloc(sizeof(char) * 256);
  read(clientFD, data, sizeof(data));
  printf("Data: %s\n", data);
}

void runDiscovery()
{
  int clientFD;
  struct sockaddr_in server;

  if ((listenFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  // configuring the server
  bzero(&server, sizeof(server));
  server.sin_port = htons(discovery.firstPort);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_pton(AF_INET, discovery.firstIP, &server.sin_addr);

  // checking if the IP address is valid
  if (inet_pton(AF_INET, discovery.firstIP, &server.sin_addr) <= 0)
  {
    printError("Invalid IP address\n");
    exit(1);
  }

  printToConsole("Socket created\n");
  // checking if the port is valid
  if (bind(listenFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printError("Error while binding\n");
    exit(1);
  }

  write(1, "Socket binded\n", strlen("Socket binded\n"));
  // listening for connections
  if (listen(listenFD, 10) < 0)
  {
    printError("Error while listening\n");
    exit(1);
  }

  while (1)
  {
    printToConsole("Waiting for connections...\n");

    clientFD = accept(listenFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }
    printToConsole("\nNew client connected !\n");

    //getSocketData(clientFD);
    
    char *buffer = malloc(sizeof(char));
    ssize_t bytesread = read(clientFD, buffer, sizeof(buffer));

    for (ssize_t i = 0; i < bytesread; i++)
    {
      printf("0x%02x ", (unsigned char)buffer[i]);
    }
    printf("\n");
    uint16_t headerLength;
    bytesread = read(clientFD, &headerLength, sizeof(headerLength));


    headerLength = ntohs(headerLength); // Convert to host byte order

    printf("Header length: %u\n", headerLength);

    char *header = malloc(headerLength + 1);
    read(clientFD, header, headerLength);
    printf("Header: %s\n", header);

    char *data = malloc(sizeof(char) * 256);
    read(clientFD, data, sizeof(data));
    printf("Data: %s\n", data);

    close(clientFD);
  }
}



int main(int argc, char *argv[])
{
  // Reprogram the SIGINT signal
  signal(SIGINT, closeProgram);

  // Check if the arguments are provided
  if (argc != 2)
  {
    printError("Error: Missing arguments\n");
    return 1;
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0)
  {
    printError("Error: File not found\n");
    return 1;
  }

  getDiscoveryFromFile(fd);

  runDiscovery();

  freeMemory();

  return 0;
}