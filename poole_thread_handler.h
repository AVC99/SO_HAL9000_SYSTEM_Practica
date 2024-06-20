#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"
#include "semaphore_v2.h"

// arnau.vives  I3_6

extern pthread_mutex_t isPooleConnectedMutex, pipeMutex;
extern int terminate;
extern Poole poole;
extern int monolithPipe[2];
extern semaphore syncMonolithSemaphore;

void *bowmanThreadHandler(void *arg);
int processBowmanMessage(SocketMessage message, int bowmanSocket);