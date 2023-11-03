#define _GNU_SOURCE
#include "read_until.h"

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


Poole savePoole(int fd)
{
  poole.servername = readUntil('\n', fd);

  //check that the servername does not contain &
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
int main(int argc, char *argv[])
{

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
  poole = savePoole(fd);

  close(fd);

  // THIS IS FOR PHASE 1 TESTING
  phaseOneTesting(poole);

  freeMemory();

  return 0;
}