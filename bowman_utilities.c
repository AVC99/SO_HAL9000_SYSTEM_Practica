#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"

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
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdint.h>

// arnau.vives joan.medina I3_6

Bowman bowman;

void setBowman(Bowman newBowman)
{
  // Copy string data, including null terminator
  bowman.username = strdup(newBowman.username);
  bowman.folder = strdup(newBowman.folder);
  bowman.ip = strdup(newBowman.ip);

  bowman.port = newBowman.port;
}

void freeUtilitiesBowman()
{
  free(bowman.username);
  free(bowman.folder);
  free(bowman.ip);
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
  char *data = malloc(255);
  ssize_t dataBytesRead = read(clientFD, data, sizeof(data));
  printf("Data bytes read: %ld\n", dataBytesRead);
   data[dataBytesRead] = '\0';
  printf("Data: %s\n", data);

 
  message.data = data;
  //free(data);

  return message;
}

void connectToDiscovery()
{
  printToConsole("CONNECT\n");

  int socketFD;
  struct sockaddr_in server;

  // Create socket
  if ((socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printError("Error creating the socket\n");
  }

  // configure server
  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(bowman.port);

  // Convert IPv4 and IPv6 addresses from text to binary form
  if (inet_pton(AF_INET, bowman.ip, &server.sin_addr) < 0)
  {
    printError("Error configuring IP\n");
  }
  // Connect to server
  if (connect(socketFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printError("Error connecting\n");
  }

  // CONNECTED TO DISCOVERY
  // send type
  printToConsole("Connected to Discovery\n");
  uint8_t type = 0x01;
  write(socketFD, &type, 1);

  // Send header length
  uint16_t headerLength = strlen("NEW_BOWMAN");
  headerLength = htons(headerLength); // Convert to network byte order
  write(socketFD, &headerLength, sizeof(headerLength));

  // Send header
  write(socketFD, "NEW_BOWMAN", strlen("NEW_BOWMAN"));

  //Send data
  write(socketFD, bowman.username, strlen(bowman.username));

  //TODO: Receive response ------------------------------------------------
  SocketMessage message = processClient(socketFD);

  free(message.header);
  free(message.data);

}

void listSongs()
{
  write(1, "LIST SONGS\n", strlen("LIST SONGS\n"));
}
void checkDownloads()
{
  write(1, "CHECK DOWNLOADS\n", strlen("CHECK DOWNLOADS\n"));
}

void clearDownloads()
{
  write(1, "CLEAR DOWNLOADS\n", strlen("CLEAR DOWNLOADS\n"));
}

void listPlaylists()
{
  write(1, "LIST PLAYLISTS\n", strlen("LIST PLAYLISTS\n"));
}
void downloadFile(char *file)
{

  write(1, "DOWNLOAD ", strlen("DOWNLOAD "));
  write(1, file, strlen(file));
  write(1, "\n", strlen("\n"));
}
void logout()
{
  printToConsole("THANKS FOR USING HAL 9000, see you soon, music lover!\n");
}