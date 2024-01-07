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
#include <pthread.h>

// arnau.vives joan.medina I3_6

Bowman bowman;
int pooleSocketFD, isPooleConnected = FALSE;
int discoverySocketFD;
pthread_t downloadThread;
char *serverIP;

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
  serverIP = strdup(token);

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
  m.header = strdup("NEW_BOWMAN");
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
  

  if ((discoverySocketFD = createAndConnectSocket(bowman.ip, bowman.port)) < 0)
  {
    printError("Error connecting to Discovery\n");
    exit(1);
  }

  // CONNECTED TO DISCOVERY
  printToConsole("Connected to Discovery\n");

  SocketMessage m;
  m.type = 0x01;
  m.headerLength = strlen("NEW_BOWMAN");
  m.header = strdup("NEW_BOWMAN");
  m.data = strdup(bowman.username);

  sendSocketMessage(discoverySocketFD, m);
  
  // Receive response
  SocketMessage response = getSocketMessage(discoverySocketFD);

  // handle response
  connectToPoole(response);

  free(response.header);
  free(response.data);
  free(m.header);
  free(m.data);

  // ASK: IDK IF I SHOULD CLOSE THE SOCKET HERE
  close(discoverySocketFD);
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
  m.header = strdup("LIST_SONGS");
  m.data = strdup("");

  sendSocketMessage(pooleSocketFD, m);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // SHOW THE SONGS IN THE CONSOLE

  for (size_t i = 0; i < strlen(response.data); i++)
  {
    if (response.data[i] == '&')
    {
      response.data[i] = '\n';
    }
  }

  char *buffer;
  asprintf(&buffer, "Songs in the Poole server:\n%s", response.data);
  printToConsole(buffer);
  free(buffer);

  // handle response
  free(response.header);
  free(response.data);
}

void checkDownloads()
{
  printToConsole("CHECK DOWNLOADS\n");

  if (isPooleConnected == FALSE)
  {
    printError("You are not connected to Poole\n");
    return;
  }

  SocketMessage m;
  m.type = 0x02;
  m.headerLength = strlen("CHECK_DOWNLOADS");
  m.header = strdup("CHECK_DOWNLOADS");
  m.data = strdup("");

  sendSocketMessage(pooleSocketFD, m);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // SHOW THE SONGS IN THE CONSOLE

  // handle response
  free(response.header);
  free(response.data);
}
/**
 * This method opens a fork and executes rm -rf to the Bowman folder
*/
void clearDownloads()
{
  printToConsole("CLEAR DOWNLOADS\n");

  int pipeFD[2];

  if (pipe(pipeFD) < 0)
  {
    printError("Error creating pipe\n");
    return;
  }
  
  pid_t pid = fork();
  
  if (pid<0){
    printError("Error creating fork\n");
    return;
  }
  else if (pid == 0)
  {
    // CHILD
    close(pipeFD[0]);
    dup2(pipeFD[1], STDOUT_FILENO);
    dup2(pipeFD[1], STDERR_FILENO);
    close(pipeFD[1]);

    // REMOVE THE FIRST CHAR OF THE FOLDER
    const char *folderPath = (bowman.folder[0] == '/') ? (bowman.folder + 1) : bowman.folder;
    char *args[] = {"rm", "-rvf", folderPath, NULL};
    execvp(args[0], args);
    exit(0);
  }
  else
  {
    // PARENT
    close(pipeFD[1]);
    // Wait for the child to finish
    waitpid(pid, NULL, 0);
    char buffer[1024];
    int bytesRead = read(pipeFD[0], buffer, 1024);
    buffer[bytesRead] = '\0';
    printToConsole(buffer);
    close(pipeFD[0]);
    wait(NULL);
  }


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
  m.header = strdup("LIST_PLAYLISTS");
  m.data = strdup("");

  sendSocketMessage(pooleSocketFD, m);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // SHOW THE PLAYLIST IN THE CONSOLE

  // handle response
  free(response.header);
  free(response.data);
}

void* downloadSong(void *arg)
{
  ThreadInfo info = *((ThreadInfo *)arg);
  char *buffer;
  asprintf(&buffer, "DOWNLOADING %s\n", info.filename);
  printToConsole(buffer);
  free(buffer);

  int fileFD = open(info.filename, O_CREAT | O_WRONLY, 0666);
  if (fileFD < 0)
  {
    printError("Error creating file\n");
    return NULL;
  }

  int pooleSocketFD = createAndConnectSocket(serverIP, DOWNLOAD_SONG_PORT);

  if (pooleSocketFD < 0)
  {
    printError("Error connecting to Poole\n");
    return NULL;
  }


  
  





  return NULL;
}

void downloadFile(char *file)
{
  char *buffer;

  if (isPooleConnected == FALSE)
  {
    printError("You are not connected to Poole\n");
    return;
  }

  SocketMessage m;
  m.type = 0x03;
  m.headerLength = strlen(DOWNLOAD_SONG);
  m.header = strdup("DOWNLOAD_SONG");
  m.data = strdup(file);

  sendSocketMessage(pooleSocketFD, m);
  free(m.header);
  free(m.data);
  printToConsole("Message sent to Poole\n");

  SocketMessage response = getSocketMessage(pooleSocketFD);

  // Start downloading 

  // TODO: FREE MEMORY
  ThreadInfo *info = malloc(sizeof(ThreadInfo));
  info->socketFD = pooleSocketFD;

  char* filename = strtok(response.data, "&");
  info->filename = strdup(filename);
  
  char *fileSize = strtok(NULL, "&");
  long long fileSizeInt = atoll(fileSize);
  info->fileSize = fileSizeInt;
  char *md5sum = strtok(NULL, "&");
  //free(md5sum);
  char *id = strtok(NULL, "&");
  int ID = atoi(id);
  info->ID = ID;

  asprintf(&buffer, "Filename: %s\nFile size: %lld\nMD5SUM: %s\nID: %d\n", filename, fileSizeInt, md5sum, ID);
  printToConsole(buffer);
  free(buffer);
  
  // TODO: MAYBE I SHOULD OPEN A NEW SOCKET FOR THE DOWNLOADING
  downloadThread = pthread_create(&downloadThread, NULL, downloadSong, info);

  // handle response
  free(response.header);
  //free(response.data);

  pthread_join(downloadThread, NULL);
}

/**
 * this has to notify Poole that its disconnecting and close all connections
 */
void logout()
{
  if (isPooleConnected == TRUE)
  {
    SocketMessage m;
    m.type = 0x06;
    m.headerLength = strlen("EXIT");
    m.header = strdup("EXIT");
    m.data = bowman.username;

    sendSocketMessage(pooleSocketFD, m);

    SocketMessage response = getSocketMessage(pooleSocketFD);

    if (response.type == 0x06 && strcmp(response.header, "CON_OK") == 0)
    {
      printToConsole("Bowman disconnected from Poole\n");
      close(pooleSocketFD);
      close(discoverySocketFD);
      isPooleConnected = FALSE;
    }
    else
    {
      printToConsole("Bowman could not disconnect from Poole\n");
    }
  }

  printToConsole("THANKS FOR USING HAL 9000, see you soon, music lover!\n");
}