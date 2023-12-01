#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"
#include "network_utils.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>

// arnau.vives joan.medina I3_6

Poole poole;
int listenFD;

Poole savePoole(int fd)
{
  write(1, "Reading configuration file...\n", strlen("Reading configuration file...\n"));

  poole.servername = readUntil('\n', fd);
  poole.servername[strlen(poole.servername) - 1] = '\0';

  // check that the servername does not contain &
  if (strchr(poole.servername, '&') != NULL)
  {
    char *newServername = malloc(strlen(poole.servername) * sizeof(char));
    int j = 0;
    for (size_t i = 0; i < strlen(poole.servername); i++)
    {
      if (poole.servername[i] != '&')
      {
        newServername[j] = poole.servername[i];
        j++;
      }
    }
    newServername[j] = '\0';
    free(poole.servername);
    poole.servername = newServername;
  }

  poole.folder = readUntil('\n', fd);
  poole.discoveryIP = readUntil('\n', fd);
  char *port = readUntil('\n', fd);
  poole.discoveryPort = atoi(port);
  free(port);
  poole.pooleIP = readUntil('\n', fd);
  port = readUntil('\n', fd);
  poole.poolePort = atoi(port);
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
  asprintf(&buffer, "Discovery IP - %s\n", poole.discoveryIP);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Discovery Port - %d\n", poole.discoveryPort);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Poole IP - %s\n", poole.pooleIP);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Poole Port - %d\n", poole.poolePort);
  write(1, buffer, strlen(buffer));
  write(1, "\n", strlen("\n"));
  free(buffer);
}

void freeMemory()
{
  free(poole.servername);
  free(poole.folder);
  free(poole.discoveryIP);
  free(poole.pooleIP);
}

void closeProgram()
{
  freeMemory();
  exit(0);
}

void createSocket()
{
  struct sockaddr_in poole_server;

  if ((listenFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    write(1, "Error creating the socket\n", strlen("Error creating the socket\n"));
    exit(1);
  }

  bzero(&poole_server, sizeof(poole_server));
  poole_server.sin_port = htons(poole.discoveryPort);
  poole_server.sin_family = AF_INET;
  poole_server.sin_addr.s_addr = inet_pton(AF_INET, poole.discoveryIP, &poole_server.sin_addr);

  if (inet_pton(AF_INET, poole.discoveryIP, (&poole_server.sin_addr)) != 1)
  {
    printToConsole("Error converting the IP address\n");
    exit(1);
  }
  printToConsole("Socket created\n");

  if (bind(listenFD, (struct sockaddr *)&poole_server, sizeof(poole_server)) < 0)
  {
    write(1, "Error binding the socket\n", strlen("Error binding the socket\n"));
    exit(1);
  }

  write(1, "Socket binded\n", strlen("Socket binded\n"));

  /*while(1)
   {
     write(1, "Waiting for connections...\n", strlen("Waiting for connections...\n"));
   }*/
}

void connectToDiscovery()
{
  int socketFD;
  struct sockaddr_in server;

  if ((socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(poole.discoveryPort);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, poole.discoveryIP, &server.sin_addr) < 0)
  {
    printError("Error configuring IP\n");
  }
  // Connect to server
  if (connect(socketFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printError("Error connecting\n");
  }

  // CONNECTED TO DISCOVERY
  printToConsole("Connected to Discovery\n");

  SocketMessage sending = {
      .type = 0x01,
      .headerLength = strlen("NEW_POOLE"),
      .header = "NEW_POOLE",
  };

  //MARK --------------------------------------------------------------------------------------
  // IDK WHY IF I PUT THE PORT LAST IT DOESNT WORK
  char *data;
  asprintf(&data, "%s&%d&%s", poole.servername, poole.poolePort, poole.pooleIP);

  sending.data = data;

  sendSocketMessage(socketFD, sending);
  free(data);
  //free(sending.header);
  //free(sending.data);

  // RECEIVE MESSAGE
  SocketMessage message = getSocketMessage(socketFD);

  // Check if the message is correct

  free(message.header);
  free(message.data);

  close(socketFD);
}

int main(int argc, char *argv[])
{
  // Reprogram the SIGINT signal
  signal(SIGINT, closeProgram);

  // Check if the arguments are provided
  if (argc < 2)
  {
    write(2, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
    return 1;
  }

  // Check if the file exists and can be opened
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0)
  {
    write(2, "Error: File not found\n", strlen("Error: File not found\n"));
    return 1;
  }

  // Save the poole information
  poole = savePoole(fd);
  close(fd);

  // THIS IS FOR PHASE 1 TESTING
  phaseOneTesting(poole);

  connectToDiscovery();

  // createSocket();

  freeMemory();

  return 0;
}