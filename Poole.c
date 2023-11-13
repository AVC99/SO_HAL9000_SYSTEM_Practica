#define _GNU_SOURCE
#include "io_utils.h"
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
typedef struct
{
  char *servername;
  char *folder;
  char *firstIP;
  int firstPort;
  char *secondIP;
  int secondPort;
} Poole;

Poole poole;
int listenFD;

Poole savePoole(int fd)
{
  write(1, "Reading configuration file...\n", strlen("Reading configuration file...\n"));

  poole.servername = readUntil('\n', fd);

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
  // REMOVE THIS
  printf("IP: %s\n", poole.firstIP);

  bzero(&poole_server, sizeof(poole_server));
  poole_server.sin_port = htons(poole.firstPort);
  poole_server.sin_family = AF_INET;
  poole_server.sin_addr.s_addr = inet_pton(AF_INET, poole.firstIP, &poole_server.sin_addr);

  int s = inet_pton(AF_INET, poole.firstIP, &poole_server.sin_addr);
  printf("INET_PTON: %d\n", s);
  if (inet_pton(AF_INET, poole.firstIP, (&poole_server.sin_addr)) != 1)
  {
    write(1, "Error converting the IP address\n", strlen("Error converting the IP address\n"));
    exit(1);
  }
  write(1, "Socket created\n", strlen("Socket created\n"));

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

  createSocket();

  freeMemory();

  return 0;
}