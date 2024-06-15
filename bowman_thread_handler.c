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
pthread_mutex_t consoleMutex;

static int downloadQueue;

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
 * @brief Lists the playlists in the Poole server no need to free the response
 * @param response the message received from the Poole server
 */
void listPlaylists_BH(SocketMessage response) {
    // SHOW THE SONG IN THE CONSOLE
    // OUTPUT IS NOT GREAT BUT IT IS WHAT IT IS

    // get the fist characters before &
    char* start = strchr(response.data, '&');

    if (start == NULL) {
        printError("Error parsing the response\n");
        return;
    }

    for (char* c = start; *c != '\0'; c++) {
        if (*c == '&' || *c == '#') {
            *c = '\n';
        }
    }

    char* buffer;
    asprintf(&buffer, "Playlists in the Poole server:\n%s", start);
    pthread_mutex_lock(&consoleMutex);
    printToConsole(buffer);
    pthread_mutex_unlock(&consoleMutex);
    free(buffer);
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
    asprintf(&buffer, "\nDownloading file %s with size %s and md5sum %s ID %ld \n", filename, size, md5sum, id);
    pthread_mutex_unlock(&consoleMutex);
    printToConsole(buffer);
    free(buffer);

    int idLength = strlen(ID) + 1;  // +1 for the & symbol in the data that is part of the pure data
    int sizeInt = atoi(size);

    int maxDataSize = BUFFER_SIZE - 3 - strlen("FILE_DATA") - idLength;
    pthread_mutex_lock(&consoleMutex);
    asprintf(&buffer, "Max data size %d\n", maxDataSize);
    printToConsole(buffer);
    free(buffer);
    pthread_mutex_unlock(&consoleMutex);
    int numberOfChunks = sizeInt / maxDataSize;

    if (sizeInt % maxDataSize != 0) {
        numberOfChunks++;
    }

    asprintf(&buffer, "Preparing to recive %d chunks\n", numberOfChunks);
    printToConsole(buffer);
    free(buffer);

    int lastChunkSize = sizeInt % (maxDataSize);

    asprintf(&buffer, "Last Chunk size %d\n", lastChunkSize);
    printToConsole(buffer);
    free(buffer);

    const char* folderPath = (bowman.folder[0] == '/') ? (bowman.folder + 1) : bowman.folder;

    char* filePath;
    asprintf(&filePath, "%s/%s", folderPath, filename);

    int fd = open(filePath, O_CREAT | O_WRONLY, 0666);
    queueMessage message;
    memset(&message, 0, sizeof(queueMessage));

    //* NO NEEED FOR ALL OF THIS DATA IS ALREADY CLEAN NO DEED TO DO ALL TEHSE CHECKS
    // ssize_t messageDataSize = sizeof(FILE_MAX_DATA_SIZE - strlen(ID) - 1);  // -1 for the & symbol in the data that is part of the pure data
    //! This is blocking

    for (int i = 0; i < numberOfChunks; i++) {
        if (msgrcv(downloadQueue, &message, sizeof(queueMessage) - sizeof(long), id, 0) < 0) {  // id 0 gets all the messages
            printError("ERROR: While reciving the message");
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

            pthread_mutex_lock(&consoleMutex);
            asprintf(&buffer, "DATA: %s\n", message.data);
            printToConsole(buffer);
            pthread_mutex_unlock(&consoleMutex);
            free(buffer);

            pthread_mutex_lock(&consoleMutex);
            asprintf(&buffer, "Writing last chunk sized (%d)\n", lastChunkSize);
            printToConsole(buffer);
            pthread_mutex_unlock(&consoleMutex);
            free(buffer);
            close(fd);
            break;
        } else {
            write(fd, message.data, maxDataSize);
            asprintf(&buffer, "Writing chunk (%d) rawDataSize:(%d) %s\n", i, maxDataSize, message.data);
            pthread_mutex_lock(&consoleMutex);
            printToConsole(buffer);
            pthread_mutex_unlock(&consoleMutex);
            free(buffer);
        }
    }
    printToConsole("Download finished\n");

    char* downloadedMD5 = getMD5sum(filePath);

    asprintf(&buffer, "Ending thread with ID %ld\n", id);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Downloaded MD5: %s\n", downloadedMD5);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Original MD5: %s\n", md5sum);
    printToConsole(buffer);
    free(buffer);

    // TODO: SEND THE MD5SUM TO THE POOLE SERVER 0x05 CHECK_OK or 0x05 CHECK_KO DATA:buit

    if (strcmp(downloadedMD5, md5sum) == 0) {
        printToConsole("MD5sums match\n");
    } else {
        printError("MD5sums do not match\n");
    }

    free(data);
    free(filePath);
    free(downloadedMD5);

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
        printToConsole("Message received from Poole\n");

        switch (response.type) {
            case 0x02: {
                if (strcmp(response.header, "SONGS_RESPONSE") == 0) {
                    listSongs_BH(response);
                } else if (strcmp(response.header, "PLAYLISTS_RESPONSE") == 0) {
                    listPlaylists_BH(response);
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

                } else if (strcmp(response.header, "FILE_DATA") == 0) {
                    printToConsole("FILE DATA\n");
                    // get the id from data and send the data to the correct thread
                    // I get ID&data
                    size_t i = 0;
                    char idString[4] = {0};  // 4 bytes for the id possibly 3 numbers and \0
                    char *b;
                    

                    // extractning the id from the response.data
                    size_t idIndex = 0;
                    while (response.data[i] != '&') {
                        idString[idIndex++] = response.data[i++];
                    }
                    if (response.data[i] == '&') {  // skip the & symbol
                        i++;
                        asprintf(&b, "I : (%ld) data:(%c)\n", i, response.data[i]);
                        printToConsole(b);
                        free(b);
                    } else {
                        printError("Error parsing the ID\n");
                    }
                    char* buffer;
                    long idLong = strtol(idString, NULL, 10);
                    size_t idLength = strlen(idString) + 1;           // +1 for the & symbol in the data that is part of the pure data
                    size_t dataSize = FILE_MAX_DATA_SIZE - idLength;  
                    pthread_mutex_lock(&consoleMutex);
                    asprintf(&buffer, "idLength (%ld) dataSize (%ld)\n", idLength, dataSize);
                    printToConsole(buffer);
                    free(buffer);
                    pthread_mutex_unlock(&consoleMutex);


                    char* data = malloc(dataSize * sizeof(char));
                    size_t j = 0;

                    while (j < dataSize) {
                        data[j++] = response.data[i++];
                    }

                    pthread_mutex_lock(&consoleMutex);
                    asprintf(&buffer, "LAST CHAR (%c) and j= (%ld)\n", data[j - 1], j);
                    printToConsole(buffer);
                    free(buffer);
                    pthread_mutex_unlock(&consoleMutex);

                    asprintf(&buffer, "ID: %ld\n", idLong);
                    printToConsole(buffer);
                    free(buffer);

                    // send the data to the correct thread
                    queueMessage message;
                    memset(&message, 0, sizeof(queueMessage));
                    message.ID = idLong;
                    message.rawDataSize = j;
                    memcpy(message.data, data, j);

                    // memcpy(message.data, data, strlen(data) + 1);

                    printToConsole("Sending the message to the queue\n");
                    if (msgsnd(downloadQueue, &message, sizeof(queueMessage) - sizeof(long), 0) < 0) {
                        printError("Error while sendig the message");
                        return NULL;
                    }
                    printToConsole("Message sent to the queue\n");
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
    // close the message queue
    msgctl(downloadQueue, IPC_RMID, NULL);
    pthread_mutex_destroy(&consoleMutex);
    return NULL;
}
