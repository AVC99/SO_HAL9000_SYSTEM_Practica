#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"
#include "network_utils.h"

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

void connectToPoole(SocketMessage message)
{
  printToConsole("CONNECT TO POOLE\n");

  char *token = strtok(message.data, "&");
  char *serverName = strdup(token);

  token = strtok(NULL, "&");
  int severPort = atoi(token);

  token = strtok(NULL, "&");
  char *serverIP = strdup(token);

  char *buffer;
  asprintf(&buffer, "POOLE server name: %s\nServer port: %d\nServer IP: %s\n", serverName, severPort, serverIP);
  printToConsole(buffer);
  free(buffer);

  int socketFD;

  if ((socketFD = createAndConnectSocket(serverIP, severPort)) < 0)
  {
    printError("Error connecting to Poole\n");
    exit(1);
  }

  // CONNECTED TO POOLE
  printToConsole("Connected to Poole\n");

}

void connectToDiscovery()
{
  printToConsole("CONNECT\n");

  int socketFD;

  if ((socketFD = createAndConnectSocket(bowman.ip, bowman.port)) < 0)
  {
    printError("Error connecting to Discovery\n");
    exit(1);
  }

  // CONNECTED TO DISCOVERY
  printToConsole("Connected to Discovery\n");

  SocketMessage m;
  m.type = 0x01;
  m.headerLength = strlen("NEW_BOWMAN");
  m.header = "NEW_BOWMAN";
  m.data = strdup(bowman.username);

  sendSocketMessage(socketFD, m);

  // When I free this it crashes and i get munmap_chunk(): invalid pointer
  // free(m.header);
  // free(m.data);

  // Receive response
  SocketMessage response = getSocketMessage(socketFD);

  // handle response

  connectToPoole(response);

  free(response.header);
  free(response.data);

  close(socketFD);
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