#define _GNU_SOURCE
#include "bowman_utilities.h"
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

// arnau.vives joan.medina I3_6

typedef struct
{
  char *username;
  char *folder;
  char *ip;
  int port;
} Bowman;

Bowman bowman;

// Read until the delimiter
char *readUntil(char del, int fd)
{

  char *chain = malloc(sizeof(char));
  char c;
  int i = 0, n;

  n = read(fd, &c, 1);

  while (c != del && n != 0)
  {
    chain[i] = c;
    i++;
    chain = realloc(chain, (sizeof(char) * (i + 1)));
    n = read(fd, &c, 1);
  }

  chain[i] = '\0';

  return chain;
}

Bowman saveBowman(int fd)
{

  bowman.username = readUntil('\n', fd);
  // check that the username does not contain &
  if (strchr(bowman.username, '&') != NULL)
  {
    char *newUsername = malloc(strlen(bowman.username) * sizeof(char));
    int j = 0;
    for (size_t i = 0; i < strlen(bowman.username); i++)
    {
      if (bowman.username[i] != '&')
      {
        newUsername[j] = bowman.username[i];
        j++;
      }
    }
    newUsername[j] = '\0';
    free(bowman.username);
    bowman.username = newUsername;
  }

  bowman.folder = readUntil('\n', fd);
  bowman.ip = readUntil('\n', fd);
  char *port = readUntil('\n', fd);
  bowman.port = atoi(port);
  free(port);

  return bowman;
}

void freeMemory()
{
  free(bowman.username);
  free(bowman.folder);
  free(bowman.ip);
}

void commandInterpreter()
{
  int bytesRead;
  char command[256];

  do
  {
    write(1, "$ ", 2);
    // READ THE COMMAND
    bytesRead = read(0, command, sizeof(command));
    if (bytesRead == 0)
    {
      break;
    }
    command[bytesRead] = '\0';
    // FORMAT THE COMMAND ADDING THE \0 AND REMOVING THE \n
    int len = strlen(command);
    if (len > 0 && command[len - 1] == '\n')
    {
      command[len - 1] = '\0';
    }

    // CHECK THE COMMAND can not use SWITCH because it does not work with strings :(
    if (strcasecmp(command, "CONNECT") == 0)
    {
      connect();
    }
    else if (strcasecmp(command, "LOGOUT") == 0)
    {
      logout();
    }
    else // IF THE COMMAND HAS MORE THAN ONE WORD
    {
      char *token = strtok(command, " ");

      if (token != NULL)
      {
        if (strcasecmp(token, "DOWNLOAD") == 0)
        {
          char *filename = strtok(NULL, " ");
          if (filename != NULL && strtok(NULL, " ") == NULL)
          {
            downloadFile(filename);
          }
          else
          {
            write(1, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
          }
        }
        else if (strcasecmp(token, "LIST") == 0)
        {
          token = strtok(NULL, " ");
          if (token != NULL && strcasecmp(token, "SONGS") == 0)
          {
            listSongs();
          }
          else if (token != NULL && strcasecmp(token, "PLAYLISTS") == 0)
          {
            listPlaylists();
          }
          else
          {
            write(1, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
          }
        }
        else if (strcasecmp(token, "CHECK") == 0)
        {
          token = strtok(NULL, " ");
          if (token != NULL && strcasecmp(token, "DOWNLOADS") == 0)
          {
            checkDownloads();
          }
          else
          {
            write(1, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
          }
        }
        else if (strcasecmp(token, "CLEAR") == 0)
        {
          token = strtok(NULL, " ");
          if (token != NULL && strcasecmp(token, "DOWNLOADS") == 0)
          {
            clearDownloads();
          }
          else
          {
            write(1, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
          }
        }
        else
        {
          write(1, "Unknown command\n", strlen("Unknown command\n"));
        }
      }
      else
      {
        write(1, "ERROR: Please input a valid command.\n", strlen("ERROR: Please input a valid command.\n"));
      }
    }
  } while (strcasecmp(command, "LOGOUT") != 0);
}

void closeProgram()
{
  freeMemory();
  exit(0);
}

void phaseOneTesting()
{
  char *buffer;
  write(1, "File read correctly:\n", strlen("File read correctly:\n"));
  asprintf(&buffer, "User - %s\n", bowman.username);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Directory - %s\n", bowman.folder);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "IP - %s\n", bowman.ip);
  write(1, buffer, strlen(buffer));
  free(buffer);
  asprintf(&buffer, "Port - %d\n", bowman.port);
  write(1, buffer, strlen(buffer));
  write(1, "\n", strlen("\n"));
  free(buffer);
}

int main(int argc, char *argv[])
{
  char *buffer;

  // Check if the arguments are provided
  if (argc < 2)
  {
    write(2, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
    return 1;
  }

  int fd = open(argv[1], O_RDONLY);

  if (fd < 0)
  {
    write(2, "Error: File not found\n", strlen("Error: File not found\n"));
    return 1;
  }
  signal(SIGINT, closeProgram);

  saveBowman(fd);

  asprintf(&buffer, "%s user initialized\n", bowman.username);
  write(1, buffer, strlen(buffer));
  free(buffer);

  // THIS IS FOR PHASE 1 TESTING
  phaseOneTesting(bowman);
  close(fd);

  commandInterpreter();

  freeMemory();

  return 0;
}