#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

volatile int terminate = FALSE;
int inputFileFd, listenBowmanFD, listenPooleFD, numPooles = 0;
Discovery discovery;
pthread_t bowmanThread, pooleThread;
pthread_mutex_t terminateMutex = PTHREAD_MUTEX_INITIALIZER, numPoolesMutex = PTHREAD_MUTEX_INITIALIZER,
                poolesMutex = PTHREAD_MUTEX_INITIALIZER;
PooleServer *pooles;
/**
 * @brief Frees the memory allocated for the Discovery struct
 */
void freeMemory() {
    free(discovery.pooleIP);
    free(discovery.bowmanIP);
}
/**
 * @brief Closes the file descriptors if they are open
 */
void closeFds() {
    if (inputFileFd != 0) {
        close(inputFileFd);
    }
    if (listenBowmanFD != 0) {
        close(listenBowmanFD);
    }
    if (listenPooleFD != 0) {
        close(listenPooleFD);
    }
}
/**
 * @brief Closes the program correctly cleaning the memory and closing the file descriptors
 */
void closeProgram() {
    // TODO: close threads using pthread_cancel
    pthread_mutex_lock(&terminateMutex);
    terminate = TRUE;
    pthread_mutex_unlock(&terminateMutex);

    pthread_join(bowmanThread, NULL);
    pthread_join(pooleThread, NULL);
    freeMemory();
    closeFds();
    exit(0);
}

void phaseOneTesting() {
    char *buffer;
    printToConsole("Discovery information:\n");
    asprintf(&buffer, "Poole IP: %s\n", discovery.pooleIP);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Bowman IP: %s\n", discovery.bowmanIP);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Poole port: %d\n", discovery.poolePort);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Bowman port: %d\n", discovery.bowmanPort);
    printToConsole(buffer);
    free(buffer);
}

void saveDiscovery(char *filename) {
    inputFileFd = open(filename, O_RDONLY);
    if (inputFileFd < 0) {
        printError("Error: File not found\n");
        exit(1);
    }
    discovery.pooleIP = readUntil('\n', inputFileFd);
    // discovery.pooleIP[strlen(discovery.pooleIP) - 1] = '\0';

    char *port = readUntil('\n', inputFileFd);
    discovery.poolePort = atoi(port);
    free(port);

    discovery.bowmanIP = readUntil('\n', inputFileFd);
    // discovery.bowmanIP[strlen(discovery.bowmanIP) - 1] = '\0';
    port = readUntil('\n', inputFileFd);
    discovery.bowmanPort = atoi(port);
    free(port);
    close(inputFileFd);
}

void *listenToBowman() {
    printToConsole("Listening to Bowman\n");

    if ((listenBowmanFD = createAndListenSocket(discovery.bowmanIP, discovery.bowmanPort)) < 0) {
        printError("Error creating Bowman socket\n");
        exit(1);
    }
    pthread_mutex_lock(&terminateMutex);
    while (terminate == FALSE) {
        pthread_mutex_unlock(&terminateMutex);

        printToConsole("Waiting for Bowman...\n");

        int bowmanSocketFD = accept(listenBowmanFD, (struct sockaddr *)NULL, NULL);

        if (bowmanSocketFD < 0) {
            printError("Error accepting Bowman\n");
            exit(1);
        }
        printToConsole("Bowman connected\n");

        SocketMessage m = getSocketMessage(bowmanSocketFD);
        printToConsole("Message received\n");

        if (strcmp(m.header, "NEW_BOWMAN") == 0) {
            printToConsole("New Bowman connected to Discovery\n");

            pthread_mutex_lock(&numPoolesMutex);
            if (numPooles == 0) {
                pthread_mutex_unlock(&numPoolesMutex);
                printError("ERROR: No Poole connected\n");
                SocketMessage response;
                response.type = 0x01;
                response.headerLength = strlen("CON_KO");
                response.header = strdup("CON_KO");
                response.data = strdup("");

                sendSocketMessage(bowmanSocketFD, response);
                free(response.header);
                free(response.data);
            } else {
                pthread_mutex_unlock(&numPoolesMutex);
                printToConsole("Searching for the Poole with less Bowmans\n");
                int minBowmans = INT_MAX, minBowmansIndex = -1;
                // Search for the Poole with less Bowmans

                pthread_mutex_lock(&poolesMutex);
                for (int i = 0; i < numPooles; i++) {
                    if (pooles[i].numOfBowmans < minBowmans) {
                        minBowmans = pooles[i].numOfBowmans;
                        minBowmansIndex = i;
                    }
                }
                char *buffer;
                asprintf(&buffer, "Poole with less Bowmans: %s\n", pooles[minBowmansIndex].pooleServername);
                printToConsole(buffer);
                free(buffer);
                pooles[minBowmansIndex].numOfBowmans++;

                printToConsole("Sending Poole information to Bowman\n");
                SocketMessage response;
                response.type = 0x01;
                response.headerLength = strlen("CON_OK");
                response.header = strdup("CON_OK");
                char *data;
                asprintf(&data, "%s&%d&%s", pooles[minBowmansIndex].pooleServername, pooles[minBowmansIndex].poolePort,
                         pooles[minBowmansIndex].pooleIP);
                response.data = strdup(data);

                sendSocketMessage(bowmanSocketFD, response);

                pthread_mutex_unlock(&poolesMutex);
                pthread_mutex_unlock(&numPoolesMutex);

                free(response.header);
                free(response.data);
                free(data);
            }
        } else if (strcmp(m.header, "EXIT") == 0) {
            printToConsole("Bowman disconnected\n");
            SocketMessage response;
            response.type = 0x06;
            response.headerLength = strlen("CON_OK");
            response.header = strdup("CON_OK");
            response.data = strdup("");

            sendSocketMessage(bowmanSocketFD, response);

            free(response.header);
            free(response.data);

            // TODO: DO LOAD BALANCER SHENANIGANS
        }

        free(m.header);
        free(m.data);

        close(bowmanSocketFD);
    }
    pthread_mutex_unlock(&terminateMutex);

    return NULL;
}

void *listenToPoole() {
    printToConsole("Listening to Poole\n");

    if ((listenPooleFD = createAndListenSocket(discovery.pooleIP, discovery.poolePort)) < 0) {
        printError("Error creating Poole socket\n");
        exit(1);
    }
    pthread_mutex_lock(&terminateMutex);
    while (terminate == FALSE) {
        pthread_mutex_unlock(&terminateMutex);
        printToConsole("Waiting for Poole...\n");

        int pooleSocketFD = accept(listenPooleFD, (struct sockaddr *)NULL, NULL);

        if (pooleSocketFD < 0) {
            printError("Error accepting Poole\n");
            exit(1);
        }

        printToConsole("Poole connected\n");

        SocketMessage m = getSocketMessage(pooleSocketFD);

        printToConsole("Message received\n");

        if (strcmp(m.header, "NEW_POOLE") == 0) {
            printToConsole("Poole connected to Discovery\n");

            // Send response
            SocketMessage response;
            response.type = 0x01;
            response.headerLength = strlen("CON_OK");
            response.header = strdup("CON_OK");
            response.data = strdup("");

            sendSocketMessage(pooleSocketFD, response);

            free(response.header);
            free(response.data);

            // Add Poole to the list
            pthread_mutex_lock(&numPoolesMutex);
            numPooles++;
            pthread_mutex_lock(&poolesMutex);
            char *dataCopy = strdup(m.data);
            char *serverName = strtok(dataCopy, "&");
            char *portStr = strtok(NULL, "&");
            char *ip = strtok(NULL, "&");

            pooles = realloc(pooles, sizeof(PooleServer) * numPooles);
            pooles[numPooles - 1].numOfBowmans = 0;
            pooles[numPooles - 1].pooleIP = strdup(ip);
            pooles[numPooles - 1].poolePort = atoi(portStr);
            pooles[numPooles - 1].pooleServername = strdup(serverName);

            free(dataCopy);
            free(m.header);
            free(m.data);

            pthread_mutex_unlock(&poolesMutex);
            pthread_mutex_unlock(&numPoolesMutex);
        }

        close(pooleSocketFD);
    }
    pthread_mutex_unlock(&terminateMutex);

    return NULL;
}

/**
 * @brief Main function
 */
int main(int argc, char *argv[]) {
    signal(SIGINT, closeProgram);
    if (argc != 2) {
        printError("Error: Wrong number of arguments\n");
        exit(1);
    }

    saveDiscovery(argv[1]);
    phaseOneTesting();

    if (pthread_create(&bowmanThread, NULL, (void *)listenToBowman, NULL) != 0) {
        printError("Error creating Bowman thread\n");
        exit(1);
    }

    if (pthread_create(&pooleThread, NULL, (void *)listenToPoole, NULL) != 0) {
        printError("Error creating Poole thread\n");
        exit(1);
    }

    pthread_join(bowmanThread, NULL);
    pthread_join(pooleThread, NULL);

    closeProgram();

    return 0;
}