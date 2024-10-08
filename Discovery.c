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

// arnau.vives I3_6

int terminate = FALSE;
int inputFileFd, listenBowmanFD, listenPooleFD, numPooles = 0;
Discovery discovery;
pthread_t bowmanThread, pooleThread;
pthread_mutex_t terminateMutex, numPoolesMutex = PTHREAD_MUTEX_INITIALIZER,
                                poolesMutex = PTHREAD_MUTEX_INITIALIZER;
PooleServer *pooles;

/**
 * @brief Frees the memory allocated for the Discovery struct
 */
void freeMemory() {
    free(discovery.pooleIP);
    free(discovery.bowmanIP);
    pthread_mutex_lock(&poolesMutex);
    for (int i = 0; i < numPooles; i++) {
        free(pooles[i].pooleIP);
        free(pooles[i].pooleServername);
        for (int j = 0; j < pooles[i].numOfBowmans; j++) {
            free(pooles[i].bowmans[j]);
        }
    }
    free(pooles);
    pthread_mutex_unlock(&poolesMutex);
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
    printToConsole("\nClosing program\n");

    pthread_mutex_lock(&terminateMutex);
    terminate = TRUE;
    printToConsole("Terminate set to TRUE\n");
    pthread_mutex_unlock(&terminateMutex);

    pthread_cancel(bowmanThread);
    pthread_cancel(pooleThread);

    pthread_join(bowmanThread, NULL);
    printToConsole("Bowman thread closed\n");
    pthread_join(pooleThread, NULL);
    printToConsole("Poole thread closed\n");

    freeMemory();
    closeFds();
    exit(0);
}
/**
 * @brief Prints the information of the Discovery struct needed for phase 1 testing
 */
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
/**
 * @brief Saves the information of the Discovery file into the Discovery struct
 *  @param filename The name of the file to read
 */
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
/**
 * @brief Listens to the Bowman server and sends the information of the Poole server with less Bowmans
 */
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
                if (minBowmansIndex != -1) {  // VALID Poole was found
                    char *buffer;
                    asprintf(&buffer, "Poole with less Bowmans: %s\n", pooles[minBowmansIndex].pooleServername);
                    printToConsole(buffer);
                    free(buffer);
                    //* UPDATE THE Poole with the new Bowman
                    pooles[minBowmansIndex].numOfBowmans++;
                    pooles[minBowmansIndex].bowmans[pooles[minBowmansIndex].numOfBowmans - 1] = strdup(m.data);

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

                    free(response.header);
                    free(response.data);
                    free(data);

                } else {  // NO Poole was found
                    SocketMessage response;
                    response.type = 0x01;
                    response.headerLength = strlen("CON_KO");
                    response.header = strdup("CON_KO");
                    response.data = strdup("");

                    sendSocketMessage(bowmanSocketFD, response);

                    free(response.header);
                    free(response.data);
                }

                pthread_mutex_unlock(&poolesMutex);
                pthread_mutex_unlock(&numPoolesMutex);
            }
        } else if (strcmp(m.header, "EXIT") == 0) {
            pthread_mutex_lock(&poolesMutex);
            printToConsole("Bowman disconnected\n");
            SocketMessage response;
            response.type = 0x06;
            response.headerLength = strlen("CON_OK");
            response.header = strdup("CON_OK");
            response.data = strdup("");

            sendSocketMessage(bowmanSocketFD, response);

            free(response.header);
            free(response.data);

            pthread_mutex_lock(&numPoolesMutex);
            for (int i = 0; i < numPooles; i++) {
                for (int j = 0; j < pooles[i].numOfBowmans; j++) {
                    if (strcmp(pooles[i].bowmans[j], m.data) == 0) {
                        free(pooles[i].bowmans[j]);
                        pooles[i].bowmans[j] = pooles[i].bowmans[pooles[i].numOfBowmans - 1];
                        pooles[i].numOfBowmans--;
                        printToConsole("Bowman removed from Poole\n");
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&numPoolesMutex);
            pthread_mutex_unlock(&poolesMutex);
        }

        free(m.header);
        free(m.data);

        close(bowmanSocketFD);
    }

    return NULL;
}
/**
 * @brief Listens to the Poole server and adds it to the list
 */
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
            char *b;
            asprintf(&b, "%s connected to Discovery\n", m.data);
            printToConsole(b);
            free(b);

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
        } else if (strcmp(m.header, "EXIT_POOLE") == 0) {
            printToConsole("Poole disconnected\n");
            SocketMessage response;
            int found = FALSE;
            char *buffer;
            pthread_mutex_lock(&numPoolesMutex);
            pthread_mutex_lock(&poolesMutex);
            for (int i = 0; i < numPooles; i++) {
                if (strcmp(pooles[i].pooleServername, m.data) == 0) {
                    asprintf(&buffer, "Poole %s removed from Discovery\n", pooles[i].pooleServername);
                    printToConsole(buffer);
                    free(buffer);
                    free(pooles[i].pooleServername);
                    free(pooles[i].pooleIP);
                    for (int j = 0; j < pooles[i].numOfBowmans; j++) {
                        free(pooles[i].bowmans[j]);
                    }
                    pooles[i] = pooles[numPooles - 1];
                    numPooles--;
                    found = TRUE;

                    break;
                }
            }
            pthread_mutex_unlock(&poolesMutex);
            pthread_mutex_unlock(&numPoolesMutex);

            if (found == TRUE) {
                response.type = 0x06;
                response.headerLength = strlen("EXIT_OK");
                response.header = strdup("EXIT_OK");
                response.data = strdup("");

                sendSocketMessage(pooleSocketFD, response);

                free(response.header);
                free(response.data);
            } else {
                response.type = 0x06;
                response.headerLength = strlen("EXIT_KO");
                response.header = strdup("EXIT_KO");
                response.data = strdup("");

                sendSocketMessage(pooleSocketFD, response);

                free(response.header);
                free(response.data);
            }

            free(m.header);
            free(m.data);

        } else if (strcmp(m.header, "EXIT_BOWMAN") == 0) {
            printToConsole("Bowman disconnected\n");
            SocketMessage response;
            response.type = 0x06;
            response.headerLength = strlen("CON_OK");
            response.header = strdup("CON_OK");
            response.data = strdup("");

            sendSocketMessage(pooleSocketFD, response);

            free(response.header);
            free(response.data);

            pthread_mutex_lock(&numPoolesMutex);
            pthread_mutex_lock(&poolesMutex);
            char *buffer;
            for (int i = 0; i < numPooles; i++) {
                for (int j = 0; j < pooles[i].numOfBowmans; j++) {
                    if (strcmp(pooles[i].bowmans[j], m.data) == 0) {
                        asprintf(&buffer, "Bowman %s removed from Poole %s\n", m.data, pooles[i].pooleServername);
                        printToConsole(buffer);
                        free(buffer);
                        free(pooles[i].bowmans[j]);
                        pooles[i].bowmans[j] = pooles[i].bowmans[pooles[i].numOfBowmans - 1];
                        pooles[i].numOfBowmans--;
                        printToConsole("Bowman removed from Poole\n");
                        break;
                    }
                }
            }
            pthread_mutex_unlock(&numPoolesMutex);
            pthread_mutex_unlock(&poolesMutex);
            free(m.header);
            free(m.data);
        }

        close(pooleSocketFD);
    }

    return NULL;
}

/**
 * @brief Main function
 * @returns 0 if the program ends correctly
 */
int main(int argc, char *argv[]) {
    signal(SIGINT, closeProgram);

    if (argc != 2) {
        printError("Error: Wrong number of arguments\n");
        exit(1);
    }

    pthread_mutex_init(&terminateMutex, NULL);
    terminate = FALSE;

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