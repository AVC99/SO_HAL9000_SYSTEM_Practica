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
int pooleSocketFD, isPooleConnected = FALSE;

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

  if ((pooleSocketFD = createAndConnectSocket(serverIP, severPort)) < 0)
  {
    printError("Error connecting to Poole\n");
    exit(1);
  }

  // CONNECTED TO POOLE
  printToConsole("Connected to Poole\n");

  SocketMessage m;
  m.type = 0x01;
  m.headerLength = strlen("NEW_BOWMAN");
  m.header = "NEW_BOWMAN";
  m.data = strdup(bowman.username);

  char *buffer2;
  asprintf(&buffer2, "Bowman username: %s\n", bowman.username);
  printToConsole(buffer2);
  free(buffer2);

  sendSocketMessage(pooleSocketFD, m);

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // handle response

  if (response.type == 0x01 && strcmp(response.header, "CON_OK") == 0)
  {
    printToConsole("Bowman connected to Poole\n");
    isPooleConnected = TRUE;
  }
  else
  {
    printToConsole("Bowman could not connect to Poole\n");
    exit(1);
  }

  free(response.header);
  free(response.data);

  free(serverName);
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
  // FIXME
  //  When I free this it crashes and i get munmap_chunk(): invalid pointer
  //  free(m.header);
  //  free(m.data);

  // Receive response
  SocketMessage response = getSocketMessage(socketFD);

  // handle response
  connectToPoole(response);

  free(response.header);
  free(response.data);

  close(socketFD);
  printToConsole("Disconnected from Discovery\n");
}

void listSongs()
{
  printToConsole("LIST SONGS\n");

  if (isPooleConnected == FALSE)
  {
    printError("You are not connected to Poole\n");
    return;
  }

  SocketMessage m;
  m.type = 0x02;
  m.headerLength = strlen("LIST_SONGS");
  m.header = "LIST_SONGS";
  m.data = "";

  sendSocketMessage(pooleSocketFD, m);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // SHOW THE SONGS IN THE CONSOLE

  // handle response
  free(response.header);
  free(response.data);
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
  printToConsole("LIST PLAYLISTS\n");

  if (isPooleConnected == FALSE)
  {
    printError("You are not connected to Poole\n");
    return;
  }

  SocketMessage m;
  m.type = 0x02;
  m.headerLength = strlen("LIST_PLAYLISTS");
  m.header = "LIST_PLAYLISTS";
  m.data = "";

  sendSocketMessage(pooleSocketFD, m);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // SHOW THE SONGS IN THE CONSOLE

  // handle response
  free(response.header);
  free(response.data);

}
void downloadFile(char *file)
{
  char *buffer;
  asprintf(&buffer, "DOWNLOAD %s\n", file);
  printToConsole(buffer);
  free(buffer);

  if (isPooleConnected == FALSE)
  {
    printError("You are not connected to Poole\n");
    return;
  }

  SocketMessage m;
  m.type = 0x03;
  m.headerLength = strlen("DOWNLOAD_SONG");
  m.header = "DOWNLOAD_SONG";
  m.data = strdup(file);

  sendSocketMessage(pooleSocketFD, m);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // handle response
  free(response.header);
  free(response.data);
}
void logout()
{
  printToConsole("THANKS FOR USING HAL 9000, see you soon, music lover!\n");
  close(pooleSocketFD);
}