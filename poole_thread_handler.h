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

extern pthread_mutex_t isPooleConnectedMutex;
extern volatile int terminate;
extern Poole poole;

void *bowmanThreadHandler(void *arg);
int processBowmanMessage(SocketMessage message, int bowmanSocket);