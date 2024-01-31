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
#include <pthread.h>

#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

extern Bowman bowman;
extern int pooleSocketFD, isPooleConnected;
extern pthread_mutex_t isPooleConnectedMutex;

/**
 * @brief Connects to the Poole server with stable connection and listens to it
 * only for download songs and download playlists bcs it's the only thing that
 * Bowman receives from Poole
 */
void* listenToPoole(void* arg) {
    free(arg);
    printToConsole("Listening to Poole\n");
    SocketMessage response;
    pthread_mutex_lock(&isPooleConnectedMutex);
    while (isPooleConnected == TRUE){
        pthread_mutex_unlock(&isPooleConnectedMutex);
        response = getSocketMessage(pooleSocketFD);
        printToConsole("Message received from Poole\n");
        // TODO: DELETE THIS IS ONLY SO THAT THE COMPLIER LETS ME COMPLIE
        if (response.type == 0x01){
            printToConsole("COMPILER LET ME COMPLIE\n");
        }
        // TODO: SAVE SONGS AND PLAYLISTS
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);

    return NULL;
}

