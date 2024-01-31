#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

extern pthread_mutex_t isPooleConnectedMutex;
extern volatile int terminate;
extern Poole poole;

/**
 * @brief Lists the .mp3 songs in the Poole folder
 * @param bowmanSocket The socket of the Bowman
 */
void listSongs(int bowmanSocket) {
    printToConsole("Listing songs\n");
    int fd[2];
    pipe(fd);
    pid_t pid = fork();

    if (pid == 0) {
        //* CHILD
        close(fd[0]);
        dup2(fd[1], STDOUT_FILENO);
        close(fd[1]);

        const char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;

        if (chdir(folderPath) < 0) {
            printError("Error changing directory\n");
            exit(1);
        }

        execlp("sh", "sh", "-c", "ls | grep .mp3", NULL);
        //! it should never reach this point
        printError("Error executing ls\n");
        exit(1);
    } else if (pid > 0) {
        //* PARENT
        close(fd[1]);

        // FIXME: IDK WHAT NUMBER TO PUT IN  THE BUFFER SIZE
        char *pipeBuffer = malloc(1000 * sizeof(char));
        ssize_t bytesRead = read(fd[0], pipeBuffer, 1000);

        if (bytesRead < 0) {
            printError("Error reading from pipe\n");
            exit(1);
        }

        pipeBuffer[bytesRead] = '\0';

        for (ssize_t i = 0; i < bytesRead; i++) {
            if (pipeBuffer[i] == '\n') {
                pipeBuffer[i] = '&';
            }
        }

        SocketMessage m;
        m.type = 0x02;
        m.headerLength = strlen("SONGS_RESPONSE");
        m.header = strdup("SONGS_RESPONSE");
        m.data = strdup(pipeBuffer);

        sendSocketMessage(bowmanSocket, m);

        free(m.header);
        free(m.data);
        free(pipeBuffer);

        close(fd[0]);
        waitpid(pid, NULL, 0);
    } else {
        printError("Error forking\n");
        exit(1);
    }
}

/**
 * @brief Processes the message received from the Bowman and returns if the thread should terminate
 * @param message The message received from the Bowman
 * @param bowmanSocket The socket of the Bowman
 */
int processBowmanMessage(SocketMessage message, int bowmanSocket) {
    printToConsole("Processing message from Bowman\n");
    switch (message.type) {
        case 0x02: {
            if (strcmp(message.header, "LIST_SONGS") == 0) {
                listSongs(bowmanSocket);

            } else if (strcmp(message.header, "LIST_PLAYLISTS") == 0) {
                // listPlaylists(bowmanSocket);
                printToConsole("LIST PLAYLISTS\n");
            } else {
                printError("Error processing message from Bowman\n");
                sendError(bowmanSocket);
            }
            break;
        }
        case 0x03: {
            if (strcmp(message.header, "DOWNLOAD_SONG") == 0) {
                // downloadSong(message.data, bowmanSocket);
                printToConsole("DOWNLOAD SONG\n");

            } else if (strcmp(message.header, "DOWNLOAD_PLAYLIST") == 0) {
                // downloadPlaylist(message.data, bowmanSocket);
                printToConsole("DOWNLOAD PLAYLIST\n");

            } else {
                printError("Error processing message from Bowman\n");
                sendError(bowmanSocket);
            }
            break;
        }
        case 0x06: {
            if (strcmp(message.header, "EXIT") == 0) {
                printToConsole("Bowman disconnected\n");
                SocketMessage response;
                response.type = 0x06;
                response.headerLength = strlen("CON_OK");
                response.header = strdup("CON_OK");
                response.data = strdup("");

                sendSocketMessage(bowmanSocket, response);

                // I DO THE THING WERE BOWMAN DISCONNECTS FROM POOLE AND DISCOVERY
                // SO AS LONG AS I END THE THREAD IM OK
                // TODO CHECK IF THE THREAD IS CORRECTLY ENDED

                free(response.header);
                free(response.data);

                return TRUE;
            } else {
                printError("Error processing message from Bowman\n");
                SocketMessage response;
                response.type = 0x06;
                response.headerLength = strlen("CON_KO");
                response.header = strdup("CON_KO");
                response.data = strdup("");

                sendSocketMessage(bowmanSocket, response);

                
                free(response.header);
                free(response.data);

                return TRUE;
            }
            break;
        }
        default: {
            printError("Error processing message from Bowman\n");
            sendError(bowmanSocket);
            return TRUE;
            break;
        }
    }

    return FALSE;
}

/**
 * @brief Handles the Bowman thread
 * @param arg The bowman socket (int)
 */
void *bowmanThreadHandler(void *arg) {
    printToConsole("Bowman thread created\n");
    int bowmanSocket = *((int *)arg);
    free(arg);
    SocketMessage m;
    m.type = 0x01;
    m.headerLength = strlen("CON_OK");
    m.header = strdup("CON_OK");
    m.data = strdup("");

    sendSocketMessage(bowmanSocket, m);

    printToConsole("Sent message to Bowman\n");

    free(m.header);
    free(m.data);

    int terminateThread = FALSE;

    while (terminateThread == FALSE) {
        printToConsole("Waiting for message from Bowman\n");
        SocketMessage message = getSocketMessage(bowmanSocket);
        printToConsole("Received message from Bowman\n");

        terminateThread = processBowmanMessage(message, bowmanSocket);

        free(message.header);
        free(message.data);
    }

    close(bowmanSocket);
    return NULL;
}
