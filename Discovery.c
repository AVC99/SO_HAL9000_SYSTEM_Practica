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

SocketMessage processClient(int clientFD)
{
  //TODO: REMOVE PRINTF's
  SocketMessage message;
  // get the type
  uint8_t type;
  ssize_t bytesread = read(clientFD, &type, sizeof(type));
  if (bytesread == sizeof(type))
  {
    printf("Type: 0x%02x\n", type);
  }
  else
  {
    printf("Error reading type\n");
  }
  message.type = type;

  // get the header length
  uint16_t headerLength;
  bytesread = read(clientFD, &headerLength, sizeof(headerLength));
  headerLength = ntohs(headerLength); // Convert to host byte order
  printf("Header length: %u\n", headerLength);
  message.headerLength = headerLength;

  // get the header
  char *header = malloc(headerLength + 1);
  read(clientFD, header, headerLength);
  header[headerLength] = '\0';
  printf("Header: %s\n", header);
  message.header = header;
  //free(header);

  // get the data
  char *data = malloc(sizeof(char) * 256);
  ssize_t dataBytesRead = read(clientFD, data, sizeof(data));
  printf("Data: %.*s\n", (int)dataBytesRead, data);
  message.data = data;
  //free(data);

  return message;
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

  printToConsole("Socket binded\n");

  // listening for connections
  if (listen(listenFD, 10) < 0)
  {
    printError("Error while listening\n");
    exit(1);
  }

  while (1)
  {
    printToConsole("\nWaiting for connections...\n");

    clientFD = accept(listenFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }
    printToConsole("\nNew client connected !\n");

    // GET SOCKET DATA
    SocketMessage message = processClient(clientFD);


    if (strcmp(message.header, "NEW_BOWMAN") == 0)
    {
      printf("NEW_BOWMAN DETECTED\n");
      SocketMessage response;
      response.type = 0x01;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "FUTURE_SERVER_NAME&FUTURE_SERVER_IP&FUTURE_SERVER_PORT";

      write(clientFD, &response.type, sizeof(response.type));
      uint16_t headerLength = htons(response.headerLength);
      write(clientFD, &headerLength, sizeof(headerLength));
      write(clientFD, response.header, response.headerLength);
      write(clientFD, response.data, strlen(response.data));
    }
  

    //TODO: CHECK WHEN I NEED TO FREE THIS MEMORY
    // FREE MEMORY FROM MESSAGE
    free(message.header);
    free(message.data);


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