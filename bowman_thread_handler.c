#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
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
pthread_mutex_t consoleMutex, chunkInfoMutex;

ChunkInfo* chunkInfo;

static int downloadQueue;
int nOfDownloadingSongs;

/**
 * @brief Lists the songs in the Poole server no need to free the response
 * @param response the message received from the Poole server
 */
void listSongs_BH(SocketMessage response) {
    // SHOW THE SONG IN THE CONSOLE
    printToConsole("\nSongs in the Poole server:\n\t-");
    char* buffer;
    for (size_t i = 0; i < strlen(response.data); i++) {
        if (response.data[i] == '&') {
            response.data[i] = '\n';
            asprintf(&buffer, "%c", response.data[i]);
            pthread_mutex_lock(&consoleMutex);
            printToConsole(buffer);
            pthread_mutex_unlock(&consoleMutex);
            free(buffer);
            if (i < strlen(response.data) - 1) {
                printToConsole("\t-");
            }

        } else {
            asprintf(&buffer, "%c", response.data[i]);

            pthread_mutex_lock(&consoleMutex);
            printToConsole(buffer);
            pthread_mutex_unlock(&consoleMutex);
            free(buffer);
        }
    }
}
/**
 * @brief Prints the Bowman command line
 */
void printBowmanCmd() {
    pthread_mutex_lock(&consoleMutex);
    printToConsole("Bowman $ ");
    pthread_mutex_unlock(&consoleMutex);
}
void printPlaylists(char* buffer) {
    char* playlistToken;
    char* rest = buffer;
    char* innerRest;

    // Skip the initial number of playlists as it's not needed for printing
    strtok_r(rest, "#", &rest);

    // Iterate through each playlist block
    while ((playlistToken = strtok_r(rest, "#", &rest))) {
        char* playlistName = strtok_r(playlistToken, "&", &innerRest);
        printf("Playlist: %s\n", playlistName);

        char* song;
        // Iterate through each song in the playlist
        while ((song = strtok_r(innerRest, "&", &innerRest))) {
            printf(" - %s\n", song);
        }
        printf("\n");  // Separate playlists for readability
    }
}
/**
 * @brief Lists the playlists in the Poole server no need to free the response
 * @param response the message received from the Poole server
 */
void listPlaylists_BH(SocketMessage response) {

    char* dataCopy = strdup(response.data);
    char* nOfBuffers = strtok(dataCopy, "#");
    int nOfBuffersInt = atoi(nOfBuffers);

    if (nOfBuffersInt > 1) {
        printPlaylists(response.data);
        for (int i = 0; i < nOfBuffersInt; i++) {
            SocketMessage m = getSocketMessage(pooleSocketFD);
            if (strcmp(m.header, "PLAYLISTS_RESPONSE") == 0) {
                printPlaylists(m.data);
            } else {
                printError("Unknown header\n");
            }
        }
    } else {
        printPlaylists(response.data);
    }

    free(dataCopy);
}
/**
 *
 */
void* downloadThreadHandler(void* arg) {
    char* data = (char*)arg;
    char* filename = strtok(data, "&");
    char* size = strtok(NULL, "&");
    char* md5sum = strtok(NULL, "&");
    char* ID = strtok(NULL, "&");

    // id now needs to be long
    long id = strtol(ID, NULL, 10);

    char* buffer;
    pthread_mutex_lock(&consoleMutex);
    asprintf(&buffer, "\nDownloading file %s with size %s and md5sum %s \n", filename, size, md5sum);
    printToConsole(buffer);
    pthread_mutex_unlock(&consoleMutex);
    free(buffer);

    printBowmanCmd();

    int idLength = strlen(ID) + 1;  // +1 for the & symbol in the data that is part of the pure data
    int sizeInt = atoi(size);

    int maxDataSize = BUFFER_SIZE - 3 - strlen("FILE_DATA") - idLength;
    int numberOfChunks = sizeInt / maxDataSize;

    if (sizeInt % maxDataSize != 0) {
        numberOfChunks++;
    }

    int chunkInfoIndex = 0;
    pthread_mutex_lock(&chunkInfoMutex);

    if (nOfDownloadingSongs == 0) {
        chunkInfo = malloc(sizeof(ChunkInfo));
        chunkInfo->totalChunks = numberOfChunks;
        chunkInfo->downloadedChunks = 0;
        chunkInfo->filename = strdup(filename);
        // pthread_mutex_lock(&consoleMutex);
        // printToConsole("First SONG\n");
        // pthread_mutex_unlock(&consoleMutex);
        nOfDownloadingSongs = 1;
    } else {
        chunkInfo = realloc(chunkInfo, sizeof(ChunkInfo) * (nOfDownloadingSongs + 1));
        chunkInfo[nOfDownloadingSongs].totalChunks = numberOfChunks;
        chunkInfo[nOfDownloadingSongs].downloadedChunks = 0;
        chunkInfoIndex = nOfDownloadingSongs;
        chunkInfo[nOfDownloadingSongs].filename = strdup(filename);
        chunkInfoIndex = nOfDownloadingSongs;
        nOfDownloadingSongs++;
        // char* buffer;
        // asprintf(&buffer, "Number of downloading songs %d\n", nOfDownloadingSongs);
        // pthread_mutex_lock(&consoleMutex);
        // printToConsole(buffer);
        // pthread_mutex_unlock(&consoleMutex);
        // free(buffer);
    }
    pthread_mutex_unlock(&chunkInfoMutex);


    int lastChunkSize = sizeInt % (maxDataSize);

    const char* folderPath = (bowman.folder[0] == '/') ? (bowman.folder + 1) : bowman.folder;

    char* filePath;
    asprintf(&filePath, "%s/%s", folderPath, filename);

    int fd = open(filePath, O_CREAT | O_WRONLY, 0666);
    queueMessage message;
    memset(&message, 0, sizeof(queueMessage));

    //! This is blocking
    for (int i = 0; i < numberOfChunks; i++) {
        if (msgrcv(downloadQueue, &message, sizeof(queueMessage) - sizeof(long), id, 0) < 0) {  // id 0 gets all the messages
            pthread_mutex_lock(&consoleMutex);
            printError("ERROR: While reciving the message");
            pthread_mutex_unlock(&consoleMutex);
            free(filePath);
            close(fd);
            return NULL;
        }

        if (i == numberOfChunks - 1 && lastChunkSize != 0) {
            char* lastChunk = malloc(lastChunkSize * sizeof(char));
            int j = 0;
            for (j = 0; j < lastChunkSize; j++) {
                lastChunk[j] = message.data[j];
            }

            write(fd, lastChunk, j);

            pthread_mutex_lock(&chunkInfoMutex);
            chunkInfo[chunkInfoIndex].downloadedChunks++;
            free(chunkInfo[chunkInfoIndex].filename);

            nOfDownloadingSongs--;

            pthread_mutex_unlock(&chunkInfoMutex);

            free(lastChunk);
            close(fd);
            break;
        } else {
            write(fd, message.data, maxDataSize);
            pthread_mutex_lock(&chunkInfoMutex);
            chunkInfo[chunkInfoIndex].downloadedChunks++;
            // printToConsole("Chunk downloaded\n");
            pthread_mutex_unlock(&chunkInfoMutex);
        }
    }

    char* downloadedMD5 = getMD5sum(filePath);

    if (strcmp(downloadedMD5, md5sum) == 0) {
        asprintf(&buffer, "%s downloaded correctly (MD5s match)\n", filename);
        pthread_mutex_lock(&consoleMutex);
        printToConsole(buffer);
        pthread_mutex_unlock(&consoleMutex);
        

        free(buffer);

        SocketMessage m;
        m.type = 0x05;
        m.headerLength = strlen("CHECK_OK");
        m.header = strdup("CHECK_OK");
        m.data = strdup("");

        sendSocketMessage(pooleSocketFD, m);
        free(m.header);
        free(m.data);

    } else {
        asprintf(&buffer, "%s downloaded incorrectly (MD5s do not match)\n", filename);
        pthread_mutex_lock(&consoleMutex);
        printToConsole(buffer);
        pthread_mutex_unlock(&consoleMutex);
        free(buffer);
        printToConsole("Bowman $ ");

        SocketMessage m;
        m.type = 0x05;
        m.headerLength = strlen("CHECK_KO");
        m.header = strdup("CHECK_KO");
        m.data = strdup("");

        sendSocketMessage(pooleSocketFD, m);
        free(m.header);
        free(m.data);
    }

    free(data);
    free(filePath);
    free(downloadedMD5);
    printBowmanCmd();

    return NULL;
}
/**
 * @brief Connects to the Poole server with stable connection and listens to it
 * @param arg not used
 */
void* listenToPoole(void* arg) {
    free(arg);
    // create the message queue for the download threads
    pthread_mutex_init(&consoleMutex, NULL);
    pthread_mutex_init(&chunkInfoMutex, NULL);

    pthread_mutex_lock(&chunkInfoMutex);
    nOfDownloadingSongs = 0;
    pthread_mutex_unlock(&chunkInfoMutex);

    downloadQueue = msgget(IPC_PRIVATE, 0600 | IPC_CREAT);
    if (downloadQueue < 0) {
        printError("Error creating the message queue\n");
        return NULL;
    }

    SocketMessage response;
    char buffer[1];
    pthread_mutex_lock(&isPooleConnectedMutex);
    while (isPooleConnected == TRUE) {
        pthread_mutex_unlock(&isPooleConnectedMutex);
        if (recv(pooleSocketFD, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT) == 0) {
            // The recv function returned 0, which means the socket has been closed
            printToConsole("Socket has been closed\n");
            break;
        }
        response = getSocketMessage(pooleSocketFD);
        // printToConsole("Message received from Poole\n");

        switch (response.type) {
            case 0x02: {
                if (strcmp(response.header, "SONGS_RESPONSE") == 0) {
                    listSongs_BH(response);
                    printBowmanCmd();
                } else if (strcmp(response.header, "PLAYLISTS_RESPONSE") == 0) {
                    listPlaylists_BH(response);
                    printBowmanCmd();
                } else {
                    printError("Unknown header\n");
                }
                break;
            }
            case 0x04: {
                if (strcmp(response.header, "NEW_FILE") == 0) {
                    printToConsole("NEW FILE\n");
                    // I get filename&size&md5sum&ID
                    // I need to open a thread to receive the file
                    // and a message queue to send the data to the thread
                    char* dataCopy = strdup(response.data);  // this is freed in the thread
                    pthread_t thread;
                    pthread_create(&thread, NULL, downloadThreadHandler, (void*)dataCopy);
                    pthread_detach(thread);
                    printBowmanCmd();

                } else if (strcmp(response.header, "FILE_DATA") == 0) {
                    // printToConsole("FILE DATA\n");
                    //  get the id from data and send the data to the correct thread
                    //  I get ID&data
                    size_t i = 0;
                    char idString[4] = {0};  // 4 bytes for the id possibly 3 numbers and \0

                    // extractning the id from the response.data
                    size_t idIndex = 0;
                    while (response.data[i] != '&') {
                        idString[idIndex++] = response.data[i++];
                    }
                    if (response.data[i] == '&') {  // skip the & symbol
                        i++;
                    } else {
                        printError("Error parsing the ID\n");
                    }

                    long idLong = strtol(idString, NULL, 10);
                    size_t idLength = strlen(idString) + 1;  // +1 for the & symbol in the data that is part of the pure data
                    size_t dataSize = FILE_MAX_DATA_SIZE - idLength;

                    char* data = malloc(dataSize * sizeof(char));
                    size_t j = 0;

                    while (j < dataSize) {
                        data[j++] = response.data[i++];
                    }

                    // send the data to the correct thread
                    queueMessage message;
                    memset(&message, 0, sizeof(queueMessage));
                    message.ID = idLong;
                    message.rawDataSize = j;
                    memcpy(message.data, data, j);

                    // memcpy(message.data, data, strlen(data) + 1);

                    if (msgsnd(downloadQueue, &message, sizeof(queueMessage) - sizeof(long), 0) < 0) {
                        printError("Error while sendig the message");
                        return NULL;
                    }
                    free(data);
                } else {
                    printError("Unknown header\n");
                }
                break;
            }
            case 0x06: {
                if (strcmp(response.header, "CON_OK") == 0) {
                    printToConsole("Poole disconnected\n");
                    pthread_mutex_lock(&isPooleConnectedMutex);
                    isPooleConnected = FALSE;
                    printToConsole("IS Pooole Connected set to FALSE\n");
                    pthread_mutex_unlock(&isPooleConnectedMutex);
                } else if (strcmp(response.header, "EXIT_POOLE") == 0) {
                    printToConsole("Poole disconnected\n");

                    SocketMessage m;
                    m.type = 0x06;
                    m.headerLength = strlen("CON_OK");
                    m.header = strdup("CON_OK");
                    m.data = strdup("");

                    sendSocketMessage(pooleSocketFD, m);

                    free(m.header);
                    free(m.data);
                    sleep(1);

                    pthread_mutex_lock(&isPooleConnectedMutex);
                    isPooleConnected = FALSE;
                    printToConsole("IS Pooole Connected set to FALSE\n");
                    pthread_mutex_unlock(&isPooleConnectedMutex);
                    close(pooleSocketFD);
                    printBowmanCmd();
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
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);
    // close the message queue and mutexes
    msgctl(downloadQueue, IPC_RMID, NULL);
    pthread_mutex_destroy(&consoleMutex);
    pthread_mutex_destroy(&chunkInfoMutex);

    return NULL;
}
