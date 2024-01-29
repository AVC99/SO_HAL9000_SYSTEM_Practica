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

int inputFileFd, terminate = FALSE, listenBowmanFD, listenPooleFD;
Discovery discovery;
pthread_t bowmanThread, pooleThread;
pthread_mutex_t terminateMutex = PTHREAD_MUTEX_INITIALIZER;

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
    pthread_mutex_lock(&terminateMutex);
    terminate = TRUE;
    pthread_mutex_unlock(&terminateMutex);

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

    while (terminate == FALSE) {
        /*pthread_mutex_lock(&terminateMutex);
        if (terminate) {
            pthread_mutex_unlock(&terminateMutex);
            pthread_exit(NULL);
        }
        pthread_mutex_unlock(&terminateMutex);*/

        printToConsole("Waiting for Bowman...\n");

        int bowmanSocketFD = accept(listenBowmanFD, (struct sockaddr *)NULL, NULL);

        if (bowmanSocketFD < 0) {
            printError("Error accepting Bowman\n");
            exit(1);
        }
        printToConsole("Bowman connected\n");

        SocketMessage m = getSocketMessage(bowmanSocketFD);
        printToConsole("Message received\n");

        char *buffer;
        asprintf(&buffer, "Message type: %d\n", m.type);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Message header length: %d\n", m.headerLength);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Message header: %s\n", m.header);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Message data: %s\n", m.data);
        printToConsole(buffer);
        free(buffer);

        if (strcmp(m.header, "NEW_BOWMAN") == 0) {
            printToConsole("Bowman connected to Discovery\n");

            // Send response
            SocketMessage response;
            response.type = 0x01;
            response.headerLength = strlen("NEW_BOWMAN");
            response.header = strdup("NEW_BOWMAN");
            response.data = strdup("OK");

            sendSocketMessage(bowmanSocketFD, response);

            free(response.header);
            free(response.data);
        }
        free(m.header);
        free(m.data);

        close(bowmanSocketFD);
    }

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

        char *buffer;
        asprintf(&buffer, "Message type: %d\n", m.type);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Message header length: %d\n", m.headerLength);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Message header: %s\n", m.header);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Message data: %s\n", m.data);
        printToConsole(buffer);
        free(buffer);

        if (strcmp(m.header, "NEW_POOLE") == 0) {
            printToConsole("Poole connected to Discovery\n");

            // Send response
            SocketMessage response;
            response.type = 0x01;
            response.headerLength = strlen("NEW_POOLE");
            response.header = strdup("NEW_POOLE");
            response.data = strdup("OK");

            sendSocketMessage(pooleSocketFD, response);

            free(response.header);
            free(response.data);
        }

        free(m.header);
        free(m.data);

        close(pooleSocketFD);
    }
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

    // Create Bowman thread
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