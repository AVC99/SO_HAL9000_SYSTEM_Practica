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
// FIX: listenFD what is Bowman? Discovery?
int listenFD;
pthread_t *bowmanThreads;
int *bowmanClientSockets;
int bowmanThreadsCount = 0;
pthread_mutex_t bowmanThreadsMutex, bowmanClientSocketsMutex;

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

  free(bowmanClientSockets);

  close(listenFD);
}

void closeThreads()
{
  for (int i = 0; i < bowmanThreadsCount; i++)
  {
    pthread_cancel(bowmanThreads[i]);
  }
}

void destroyMutexes()
{
  pthread_mutex_destroy(&bowmanThreadsMutex);
  pthread_mutex_destroy(&bowmanClientSocketsMutex);
}

void closeProgram()
{
  freeMemory();
  closeThreads();
  destroyMutexes();
  exit(0);
}

int proccessBowmanMessage(SocketMessage message, int clientFD)
{
  switch (message.type)
  {
  case 0x02:
    if (strcmp(message.header, "LIST_SONGS") == 0)
    {
      // TODO: HERE GOES FORK AND EXEC
      int pipefd[2];
      if (pipe(pipefd) == -1)
      {
        printError("Error while creating pipe\n");
        exit(1);
      }

      pid_t pid = fork();

      if (pid < 0)
      {
        printError("Error while forking\n");
        exit(1);
      }
      if (pid == 0)
      {
        char *buffer;
        asprintf(&buffer, "FOLDER %s\n", poole.folder);
        printToConsole(buffer);
        free(buffer);
        // CHILD
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to write end of pipe
        close(pipefd[1]);               // Close write end of pipe now that it's been duplicated

        chdir(poole.folder);
        execlp("ls", "ls", NULL);
        exit(1);
      }
      else
      {
        // PARENT
        int status;
        waitpid(pid, &status, 0);
        close(pipefd[1]);

        // IDK WHAT NUMBER TO PUT HERE
        char *buffer = malloc((255 - 3 - strlen("SONGS_RESPONSE")) * sizeof(char));

        ssize_t numRead = read(pipefd[0], message.data, sizeof(buffer) - 1);
        if (numRead == -1)
        {
          printError("Error while reading from pipe\n");
          exit(1);
        }
        message.data[numRead] = '\0';

        SocketMessage response;
        response.type = 0x02;
        response.headerLength = strlen("SONGS_RESPONSE");
        response.header = "SONGS_RESPONSE";
        response.data = buffer;

        sendSocketMessage(clientFD, response);

        free(message.header);
        free(message.data);
      }
    }
    else if (strcmp(message.header, "LIST_PLAYLISTS") == 0)
    {

      // TODO: HERE GOES FORK AND EXEC
      SocketMessage response;
      response.type = 0x02;
      response.headerLength = strlen("PLAYLIST_RESPONSE");
      response.header = "PLAYLIST_RESPONSE";
      response.data = "playlist1&playlist2&playlist3";

      sendSocketMessage(clientFD, response);

      free(message.header);
      free(message.data);
    }
    else
    {
      printError("ERROR while connecting to Bowman\n");

      sendError(clientFD);
    }
    break;

  case 0x03:
  {
    char *buffer;
    if (strcmp(message.header, DOWNLOAD_SONG) == 0)
    {

      asprintf(&buffer, "DOWNLOAD_SONG %s\n", message.data);
      printToConsole(buffer);
      free(buffer);

      SocketMessage response;
      response.type = 0x03;
      response.headerLength = strlen("DOWNLOAD_SONG_RESPONSE");
      response.header = "DOWNLOAD_SONG_RESPONSE";
      response.data = "song1data";

      sendSocketMessage(clientFD, response);

      free(message.header);
      free(message.data);
    }
    else if (strcmp(message.header, "DOWNLOAD_PLAYLIST") == 0)
    {
      asprintf(&buffer, "DOWNLOAD_PLAYLIST %s\n", message.data);
      printToConsole(buffer);
      free(buffer);

      SocketMessage response;
      response.type = 0x03;
      response.headerLength = strlen("DOWNLOAD_PLAYLIST_RESPONSE");
      response.header = "DOWNLOAD_PLAYLIST_RESPONSE";
      response.data = "playlist1data";

      sendSocketMessage(clientFD, response);

      free(message.header);
      free(message.data);
    }
    else
    {
      sendError(clientFD);
      printError("ERROR while connecting to Bowman\n");
    }
    break;
  }

  case 0x06:
    if (strcmp(message.header, "EXIT") == 0)
    {
      printToConsole("EXIT DETECTED\n");

      // HERE I RECIVE THE EXIT MESSAGE WITH USERNAME
      // I GUESS I HAVE TO SEND A MESSAGE TO DISCOVERY TO REMOVE THE BOWMAN

      // MESSAGE FOR BOWMAN
      SocketMessage response;

      response.type = 0x06;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "";

      sendSocketMessage(clientFD, response);

      free(message.header);
      free(message.data);

      // TODO: MESSAGE FOR DISCOVERY

      return TRUE;
    }

    break;

  default:
    printError("IDK WHAT MESSAGE IS THIS\n");
    sendError(clientFD);
    return TRUE;
    break;
  }

  return FALSE;
}

void *bowmanThreadHandler(void *arg)
{
  int bowmanFD = *((int *)arg);
  free(arg);

  int exit = FALSE;
  SocketMessage message;

  printToConsole("Bowman thread created\n");
  printToConsole("Listening for thread bowman messages...\n");

  while (exit == FALSE)
  {
    bzero(&message, sizeof(message));

    message = getSocketMessage(bowmanFD);
    printToConsole("Bowman message received\n");

    exit = proccessBowmanMessage(message, bowmanFD);
    bzero(&message, sizeof(message));

    free(message.header);
    free(message.data);
  }

  return NULL;
}

void listenForBowmans()
{
  int listenBowmanFD;

  if ((listenBowmanFD = createAndListenSocket(poole.pooleIP, poole.poolePort)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  while (1)
  {

    printToConsole("\nWaiting for Bowman connections...\n");

    int clientFD = accept(listenBowmanFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }

    printToConsole("Bowman connected\n");

    SocketMessage message = getSocketMessage(clientFD);

    switch (message.type)
    {
    case 0x01:
      if (strcmp(message.header, "NEW_BOWMAN") == 0)
      {
        printToConsole("NEW_BOWMAN DETECTED\n");

        SocketMessage response;
        response.type = 0x01;
        response.headerLength = strlen("CON_OK");
        response.header = "CON_OK";
        response.data = "";

        sendSocketMessage(clientFD, response);
        char *buffer;
        asprintf(&buffer, "Bowman clientFD: %d\n", clientFD);
        printToConsole(buffer);
        free(buffer);

        // MARK:

        // FIXME: THIS IS NOT WORKING
        //  Open a thread for the bowman
        pthread_t bowmanThread;
        int *FDPointer = malloc(sizeof(int));
        *FDPointer = clientFD;

        if (pthread_create(&bowmanThread, NULL, bowmanThreadHandler, FDPointer) != 0)
        {
          printError("Error creating bowman thread\n");
          close(clientFD);
          exit(1);
        }

        bowmanThreadsCount++;
        pthread_mutex_lock(&bowmanThreadsMutex);
        bowmanThreads = realloc(bowmanThreads, bowmanThreadsCount * sizeof(pthread_t));
        bowmanThreads[bowmanThreadsCount - 1] = bowmanThread;
        pthread_mutex_unlock(&bowmanThreadsMutex);
        pthread_mutex_lock(&bowmanClientSocketsMutex);
        bowmanClientSockets = realloc(bowmanClientSockets, bowmanThreadsCount * sizeof(int));
        bowmanClientSockets[bowmanThreadsCount - 1] = listenBowmanFD;
        pthread_mutex_unlock(&bowmanClientSocketsMutex);

        free(message.header);
        free(message.data);
      }
      else
      {
        printError("ERROR while connecting to Bowman\n");

        SocketMessage response;
        response.type = 0x01;
        response.headerLength = strlen("CON_KO");
        response.header = "CON_KO";
        response.data = "";

        sendSocketMessage(clientFD, response);
      }
      break;

    default:
      // TODO: HANDLE DEFAULT
      break;
    }
  }
}

void connectToDiscovery()
{
  // TODO: REFACTOR THIS TO NEW METHODS

  int socketFD;

  if ((socketFD = createAndConnectSocket(poole.discoveryIP, poole.discoveryPort)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  // CONNECTED TO DISCOVERY
  printToConsole("Connected to Discovery\n");

  SocketMessage sending = {
      .type = 0x01,
      .headerLength = strlen("NEW_POOLE"),
      .header = "NEW_POOLE",
  };

  // MARK --------------------------------------------------------------------------------------
  //  IDK WHY IF I PUT THE PORT LAST IT DOESNT WORK
  char *data;
  asprintf(&data, "%s&%d&%s", poole.servername, poole.poolePort, poole.pooleIP);

  sending.data = data;

  sendSocketMessage(socketFD, sending);
  free(data);
  // free(sending.header);
  // free(sending.data);

  // RECEIVE MESSAGE
  SocketMessage message = getSocketMessage(socketFD);

  switch (message.type)
  {
  case 0x01:
    if (strcmp(message.header, "CON_OK") == 0)
    {
      listenForBowmans();
    }
    else if (strcmp(message.header, "CON_KO") == 0)
    {
      printError("ERROR while connecting to Discovery\n");
    }
    break;

  default:
    break;
  }
  printToConsole("Discovery message received\n");
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

  // initialize mutexes
  pthread_mutex_init(&bowmanThreadsMutex, NULL);
  pthread_mutex_init(&bowmanClientSocketsMutex, NULL);

  connectToDiscovery();

  closeProgram();

  return 0;
}