#define _GNU_SOURCE
#include "io_utils.h"
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

void connect()
{
  write(1, "CONNECT\n", strlen("CONNECT\n"));
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