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

// arnau.vives joan.medina I3_6

void setBowman(Bowman bowman);
void connectToDiscovery();
void listSongs();
void checkDownloads();
void clearDownloads();
void listPlaylists();
void downloadFile(char *file);
void logout();
void freeUtilitiesBowman();