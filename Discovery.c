#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"
#include "network_utils.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>

// arnau.vives joan.medina I3_6

Discovery discovery;
int listenPooleFD, listenBowmanFD;

/**
 * Saves the discovery information from the file
 */
void getDiscoveryFromFile(int fd)
{
  printToConsole("Reading configuration file...\n");
  discovery.pooleIP = readUntil('\n', fd);
  char *firstPort = readUntil('\n', fd);
  discovery.poolePort = atoi(firstPort);
  free(firstPort);
  discovery.bowmanIP = readUntil('\n', fd);
  char *secondPort = readUntil('\n', fd);
  discovery.bowmanPort = atoi(secondPort);
  free(secondPort);

  close(fd);
}

/**
 * Frees all the memory allocated from the global Discovery
 */
void freeMemory()
{
  free(discovery.pooleIP);
  free(discovery.bowmanIP);

  close(listenPooleFD);
}

/**
 * Closes the program
 */
void closeProgram()
{
  freeMemory();
  exit(0);
}

void listenToBowman()
{

  printToConsole("Listening to Bowman...\n");

  int clientFD;
  struct sockaddr_in server;

  if ((listenBowmanFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  printf("%s\n", discovery.bowmanIP);

  // configuring the server
  bzero(&server, sizeof(server));
  server.sin_port = htons(discovery.poolePort);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_pton(AF_INET, discovery.pooleIP, &server.sin_addr);

  // checking if the IP address is valid
  if (inet_pton(AF_INET, discovery.pooleIP, &server.sin_addr) < 0)
  {
    printError("Invalid IP address\n");
    exit(1);
  }

  printToConsole("Socket created\n");
  // checking if the port is valid
  if (bind(listenBowmanFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printError("Error while binding\n");
    exit(1);
  }

  printToConsole("Socket binded\n");

  // listening for connections
  if (listen(listenBowmanFD, 10) < 0)
  {
    printError("Error while listening\n");
    exit(1);
  }

  while (1)
  {
    printToConsole("\nWaiting for connections...\n");

    clientFD = accept(listenBowmanFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }
    printToConsole("\nNew client connected !\n");

    // GET SOCKET DATA
    SocketMessage message = getSocketMessage(clientFD);

    if (strcmp(message.header, "NEW_BOWMAN") == 0)
    {
      printToConsole("NEW_BOWMAN DETECTED\n");
      SocketMessage response;
      response.type = 0x01;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "FUTURE_SERVER_NAME&FUTURE_SERVER_IP&FUTURE_SERVER_PORT";

      sendSocketMessage(clientFD, response);

      // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY i got munmap_chunk(): invalid pointer
      // free(response.header);
      // free(response.data);
    }
    else if (strcmp(message.header, "NEW_POOLE") == 0)
    {
      printToConsole("NEW_POOLE DETECTED\n");
      SocketMessage response;
      response.type = 0x01;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "";

      sendSocketMessage(clientFD, response);

      // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY i got munmap_chunk(): invalid pointer
      // free(response.header);
      // free(response.data);
    }

    // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY
    //  FREE MEMORY FROM MESSAGE
    free(message.header);
    free(message.data);

    close(clientFD);
  }
}

void listenToPoole()
{
  int clientFD;
  struct sockaddr_in server;

  printToConsole("Listening to Poole...\n");

  if ((listenPooleFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  printf("%s\n", discovery.pooleIP);

  // configuring the server
  bzero(&server, sizeof(server));
  server.sin_port = htons(discovery.poolePort);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_pton(AF_INET, discovery.pooleIP, &server.sin_addr);

  // checking if the IP address is valid
  if (inet_pton(AF_INET, discovery.pooleIP, &server.sin_addr) < 0)
  {
    printError("Invalid IP address\n");
    exit(1);
  }

  printToConsole("Socket created\n");
  // checking if the port is valid
  if (bind(listenPooleFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printError("Error while binding\n");
    exit(1);
  }

  printToConsole("Socket binded\n");

  // listening for connections
  if (listen(listenPooleFD, 10) < 0)
  {
    printError("Error while listening\n");
    exit(1);
  }

  while (1)
  {
    printToConsole("\nWaiting for connections...\n");

    clientFD = accept(listenPooleFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }
    printToConsole("\nNew client connected !\n");

    // GET SOCKET DATA
    SocketMessage message = getSocketMessage(clientFD);

    // THIS WILL NOT BE USED ANYMORE ITS BETTER TO FILTER FIRST BY MESSAGE TYPE FIRST

    if (strcmp(message.header, "NEW_BOWMAN") == 0)
    {
      printToConsole("NEW_BOWMAN DETECTED\n");
      SocketMessage response;
      response.type = 0x01;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "FUTURE_SERVER_NAME&FUTURE_SERVER_IP&FUTURE_SERVER_PORT";

      sendSocketMessage(clientFD, response);

      // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY i got munmap_chunk(): invalid pointer
      // free(response.header);
      // free(response.data);
    }
    else if (strcmp(message.header, "NEW_POOLE") == 0)
    {
      printToConsole("NEW_POOLE DETECTED\n");
      SocketMessage response;
      response.type = 0x01;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "";

      sendSocketMessage(clientFD, response);

      // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY i got munmap_chunk(): invalid pointer
      // free(response.header);
      // free(response.data);
    }

    // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY
    //  FREE MEMORY FROM MESSAGE
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

  listenToPoole();

  // listenToBowman();

  freeMemory();

  return 0;
}