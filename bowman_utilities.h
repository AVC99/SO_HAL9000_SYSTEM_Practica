#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

// arnau.vives joan.medina I3_6

extern Bowman bowman;

int connectToDiscovery(int isExit);
void listSongs();
void checkDownloads();
void clearDownloads();
void listPlaylists();
void downloadFile(char *file);
void logout();
void incrementDownloadCount(const char *songName);