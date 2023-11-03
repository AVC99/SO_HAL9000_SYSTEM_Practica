#define _GNU_SOURCE
#include "read_until.h"
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

void connect();
void listSongs();
void checkDownloads();
void clearDownloads();
void listPlaylists();
void downloadFile(char *file);
void logout();