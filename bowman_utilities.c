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

void connectToDiscovery()
{
  write(1, "CONNECT\n", strlen("CONNECT\n"));
  int socketFD;
  struct sockaddr_in server;

  if ((socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
  {
    printError("Error creating the socket\n");
  }

  bzero(&server, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons(bowman.port);

  if (inet_pton(AF_INET, bowman.ip, &server.sin_addr) < 0)
  {
    printError("Error configuring IP\n");
  }
  if (connect(socketFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printError("Error connecting\n");
  }
   printToConsole("Connected to Discovery\n");

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
  write(1, "THANKS FOR USING HAL 9000, see you soon, music lover!\n", strlen("THANKS FOR USING HAL 9000, see you soon, music lover!\n"));
}