#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"

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

void freeMemory()
{
  free(poole.servername);
  free(poole.folder);
  free(poole.firstIP);
  free(poole.secondIP);
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
  poole_server.sin_port = htons(poole.firstPort);
  poole_server.sin_family = AF_INET;
  poole_server.sin_addr.s_addr = inet_pton(AF_INET, poole.firstIP, &poole_server.sin_addr);

  if (inet_pton(AF_INET, poole.firstIP, (&poole_server.sin_addr)) != 1)
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

SocketMessage processClient(int clientFD)
{
  // TODO: REMOVE PRINTF's
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

  // get the data
  char *data = malloc(255);
  //char *data = readUntil(clientFD, '\n');
  ssize_t dataBytesRead = read(clientFD, data, 255);
  printf("Data bytes read: %ld\n", dataBytesRead);
  data[dataBytesRead] = '\0';
  printf("Data: %s\n", data);

  message.data = data;
  
  return message;
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
  server.sin_port = htons(poole.firstPort);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, poole.firstIP, &server.sin_addr) < 0)
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

  // SEND MESSAGE
  uint8_t type = 0x01;
  write(socketFD, &type, sizeof(type));

  // Send header length
  uint16_t headerLength = strlen("NEW_POOLE");
  headerLength = htons(headerLength); // Convert to network byte order
  write(socketFD, &headerLength, sizeof(headerLength));

  // Send header
  write(socketFD, "NEW_POOLE", strlen("NEW_POOLE"));

  // Send data
  char *data;
  asprintf(&data, "%s&%s", poole.servername, poole.secondIP);
  printToConsole(data);
  // asprintf(&data, "%s&%s&%d", poole.servername, poole.secondIP, poole.secondPort);
  printf("Data: %s\n", data);
  write(socketFD, data, strlen("127.0.0.1&Smyslov\n"));
  printf("Data sent\n");
  free(data);

  // RECEIVE MESSAGE
  SocketMessage message = processClient(socketFD);

  // Check if the message is correct

  free(message.header);
  free(message.data);
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