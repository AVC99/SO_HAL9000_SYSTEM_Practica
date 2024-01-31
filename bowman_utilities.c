#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "bowman_thread_handler.h"
#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

extern Bowman bowman;
int discoverySocketFD, pooleSocketFD, isPooleConnected = FALSE;
pthread_mutex_t isPooleConnectedMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t listenThread;

/**
 * @brief Connects to the Poole server with stable connection
 */
void connectToPoole(SocketMessage response) {
    printToConsole("Connecting to Poole\n");
    char *dataCopy = strdup(response.data);
    char *pooleServename = strtok(dataCopy, "&");
    char *poolePort = strtok(NULL, "&");
    char *pooleIP = strtok(NULL, "&");

    printToConsole("Poole servername: ");
    printToConsole(pooleServename);
    printToConsole("\n");

    printToConsole("Poole port: ");
    printToConsole(poolePort);
    printToConsole("\n");

    printToConsole("Poole IP: ");
    printToConsole(pooleIP);
    printToConsole("\n");

    int poolePortInt = atoi(poolePort);

    // CONNECT TO POOLE
    if ((pooleSocketFD = createAndConnectSocket(pooleIP, poolePortInt)) < 0) {
        printError("Error connecting to Poole\n");
        exit(1);
    }

    printToConsole("Connected to Poole\n");
    free(dataCopy);

    // Send message to Poole
    SocketMessage m;
    m.type = 0x01;
    m.headerLength = strlen("NEW_BOWMAN");
    m.header = strdup("NEW_BOWMAN");
    m.data = strdup(bowman.username);

    sendSocketMessage(pooleSocketFD, m);

    free(m.header);
    free(m.data);

    printToConsole("Sent message to Poole\n");

    SocketMessage pooleResponse = getSocketMessage(pooleSocketFD);

    printToConsole("Received message from Poole\n");

    if (pooleResponse.type == 0x01 && strcmp(pooleResponse.header, "CON_OK") == 0) {
        printToConsole("Poole connected to Bowman\n");
        pthread_mutex_lock(&isPooleConnectedMutex);
        isPooleConnected = TRUE;
        pthread_mutex_unlock(&isPooleConnectedMutex);

        // Start listening to Poole no need to pass the socket file descriptor to the thread
        // because it is a global variable
        if (pthread_create(&listenThread, NULL, listenToPoole, NULL) != 0) {
            printError("Error creating thread\n");
            close(pooleSocketFD);
            exit(1);
        }

    } else {
        printError("Error connecting to Poole\n");
        exit(1);
    }
    free(pooleResponse.header);
    free(pooleResponse.data);

    // IMPORTANT: response is freed in the main function
}

/**
 * @brief Connects to the Discovery server with unstable connection
 */
int connectToDiscovery(int isExit) {
    printToConsole("CONNECT\n");

    if ((discoverySocketFD = createAndConnectSocket(bowman.ip, bowman.port)) < 0) {
        printError("Error connecting to Discovery\n");
        exit(1);
    }

    // CONNECTED TO DISCOVERY
    printToConsole("Connected to Discovery\n");

    SocketMessage m;
    if (isExit == FALSE) {
        m.type = 0x01;
        m.headerLength = strlen("NEW_BOWMAN");
        m.header = strdup("NEW_BOWMAN");
        m.data = strdup(bowman.username);
    } else if (isExit == TRUE) {
        m.type = 0x06;
        m.headerLength = strlen("EXIT");
        m.header = strdup("EXIT");
        m.data = strdup(bowman.username);
    }

    sendSocketMessage(discoverySocketFD, m);
    free(m.header);
    free(m.data);

    printToConsole("Sent message to Discovery\n");
    // Receive response
    SocketMessage response = getSocketMessage(discoverySocketFD);

    // handle response
    if (isExit == FALSE) {
        connectToPoole(response);
        free(response.header);
        free(response.data);
        close(discoverySocketFD);

        return FALSE;
    } else if (isExit == TRUE) {
        if (response.type == 0x06 && strcmp(response.header, "CON_OK") == 0) {
            printToConsole("Bowman disconnected from Discovery\n");
        } else if (response.type == 0x06 && strcmp(response.header, "CON_KO") == 0) {
            printError("Error disconnecting from Discovery\n");
            exit(1);
        }
        free(response.header);
        free(response.data);
        close(discoverySocketFD);

        return TRUE;
    }

    //! It should never reach this point
    free(response.header);
    free(response.data);
    return -1;
}
/**
 * @brief Lists the songs in the Poole server in the Bowman console
 */
void listSongs() {
    printToConsole("LIST SONGS\n");

    pthread_mutex_lock(&isPooleConnectedMutex);
    if (isPooleConnected == FALSE) {
        pthread_mutex_unlock(&isPooleConnectedMutex);
        printError("You are not connected to Poole\n");
        return;
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);

    SocketMessage m;

    m.type = 0x02;
    m.headerLength = strlen("LIST_SONGS");
    m.header = strdup("LIST_SONGS");
    m.data = strdup("");

    sendSocketMessage(pooleSocketFD, m);

    free(m.header);
    free(m.data);

    printToConsole("Message sent to Poole\n");
}
void checkDownloads() {
    printToConsole("CHECK DOWNLOADS\n");
}
void clearDownloads() {
    printToConsole("CLEAR DOWNLOADS\n");
}
void listPlaylists() {
    printToConsole("LIST PLAYLISTS\n");
}
void downloadFile(char *file) {
    printToConsole("DOWNLOAD FILE\n");
    char *buffer;
    asprintf(&buffer, "Downloading file: %s\n", file);
    printToConsole(buffer);
    free(buffer);
}
void logout() {
    printToConsole("LOGOUT\n");
    pthread_mutex_lock(&isPooleConnectedMutex);
    if (isPooleConnected == TRUE) {
        pthread_mutex_unlock(&isPooleConnectedMutex);
        SocketMessage m;
        m.type = 0x06;
        m.headerLength = strlen("EXIT");
        m.header = strdup("EXIT");
        m.data = strdup(bowman.username);

        sendSocketMessage(pooleSocketFD, m);

        free(m.header);
        free(m.data);

        pthread_join(listenThread, NULL);
        close(pooleSocketFD);
        if (connectToDiscovery(TRUE) == TRUE) {
            printToConsole("Bowman disconnected from Discovery correctly\n");
        }
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);
}
