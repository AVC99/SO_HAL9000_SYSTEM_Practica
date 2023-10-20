#define _GNU_SOURCE
#include "connect.h"

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

typedef struct
{
  char *username;
  char *folder;
  char *ip;
  int port;
} Bowman;

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

// TODO CHECK IF THE FILE IS CORRECTLY FORMATTED// NEED TO FREE THE MEMORY
Bowman saveBowman(int fd)
{
  Bowman bowman;

  bowman.username = readUntil('\n', fd);
  bowman.folder = readUntil('\n', fd);
  bowman.ip = readUntil('\n', fd);
  char *port = readUntil('\n', fd);
  bowman.port = atoi(port);
  free(port);

  // check that the username does not contain &
  if (strchr(bowman.username, '&') != NULL)
  {
    write(2, "Error: Invalid username\n", strlen("Error: Invalid username\n"));
    exit(1);
  }

  return bowman;
}

// TODO: Minor issue the command not found output is not the same as the one in the
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
      write(1, "THANKS FOR USING HAL 9000, see you soon, music lover!\n", strlen("THANKS FOR USING HAL 9000, see you soon, music lover!\n"));
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
            write(1, "DOWNLOAD ", strlen("DOWNLOAD "));
            write(1, filename, strlen(filename));
            write(1, "\n", strlen("\n"));
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
            write(1, "LIST SONGS\n", strlen("LIST SONGS\n"));
          }
          else if (token != NULL && strcasecmp(token, "PLAYLISTS") == 0)
          {
            write(1, "LIST PLAYLISTS\n", strlen("LIST PLAYLISTS\n"));
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
            write(1, "CHECK DOWNLOADS\n", strlen("CHECK DOWNLOADS\n"));
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
            write(1, "CLEAR DOWNLOADS\n", strlen("CLEAR DOWNLOADS\n"));
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

void phaseOneTesting(Bowman bowman)
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

  Bowman bowman = saveBowman(fd);

  asprintf(&buffer, "%s user initialized\n", bowman.username);
  write(1, buffer, strlen(buffer));
  free(buffer);

  // THIS IS FOR PHASE 1 TESTING
  phaseOneTesting(bowman);

  commandInterpreter();

  free(bowman.username);
  free(bowman.folder);
  free(bowman.ip);

  close(fd);

  return 0;
}