#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
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

extern Bowman bowman;
extern int pooleSocketFD, isPooleConnected;
extern pthread_mutex_t isPooleConnectedMutex;

/**
 * @brief Lists the songs in the Poole server no need to free the response
 * @param response the message received from the Poole server
 */
void listSongs_BH(SocketMessage response) {
    // SHOW THE SONG IN THE CONSOLE

    for (size_t i = 0; i < strlen(response.data); i++) {
        if (response.data[i] == '&') {
            response.data[i] = '\n';
        }
    }

    char* buffer;
    asprintf(&buffer, "Songs in the Poole server:\n%s", response.data);
    printToConsole(buffer);
    free(buffer);
}

/**
 * @brief Connects to the Poole server with stable connection and listens to it
 * @param arg not used
 */
void* listenToPoole(void* arg) {
    free(arg);
    SocketMessage response;
    pthread_mutex_lock(&isPooleConnectedMutex);
    while (isPooleConnected == TRUE) {
        pthread_mutex_unlock(&isPooleConnectedMutex);
        response = getSocketMessage(pooleSocketFD);
        printToConsole("Message received from Poole\n");

        switch (response.type) {
            case 0x02: {
                if (strcmp(response.header, "SONGS_RESPONSE") == 0) {
                    listSongs_BH(response);
                } else if (strcmp(response.header, "LIST_PLAYLIST") == 0) {
                    // checkDownloads(response);
                    printToConsole("LIST_PLAYLIST\n");
                } else {
                    printError("Unknown header\n");
                }
                break;
            }
            case 0x03: {
                if (strcmp(response.header, "DOWNLOAD_SONG") == 0) {
                    // downloadFile(response);
                    printToConsole("DOWNLOAD_SONG\n");
                } else if (strcmp(response.header, "DOWNLOAD_PLAYLIST") == 0) {
                    // downloadFile(response);
                    printToConsole("DOWNLOAD_PLAYLIST\n");
                } else {
                    printError("Unknown header\n");
                }
                break;
            }
            case 0x06:{
                if (strcmp(response.header, "CON_OK") == 0) {
                    printToConsole("Poole disconnected\n");
                    pthread_mutex_lock(&isPooleConnectedMutex);
                    isPooleConnected = FALSE;
                    pthread_mutex_unlock(&isPooleConnectedMutex);
                } else {
                    printError("Unknown header\n");
                }
                break;
            }
            default: {
                printError("Unknown type\n");
                break;
            }
        }
        free(response.header);
        free(response.data);
        // TODO : FIX THIS IT ACTUALLY APEARS HERE AND IN MAIN BOWMAN MINOR BUG
        printToConsole("Bowman> ");
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);

    return NULL;
}
