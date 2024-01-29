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

#include "io_utils.h"
#include "struct_definitions.h"
#include "network_utils.h"

extern Bowman bowman;
int discoverySocketFD;

/**
 * @brief Connects to the Poole server with stable connection
*/
void connectToPoole(SocketMessage response){
    printToConsole("Connecting to Poole\n");
    char *buffer;
    asprintf(&buffer, "Response data: %s\n", response.data);
    printToConsole(buffer);
    free(buffer);
}

/**
 * @brief Connects to the Discovery server with unstable connection
*/
void connectToDiscovery(){
    printToConsole("CONNECT\n");


    if ((discoverySocketFD = createAndConnectSocket(bowman.ip, bowman.port)) < 0) {
        printError("Error connecting to Discovery\n");
        exit(1);
    }

    // CONNECTED TO DISCOVERY
    printToConsole("Connected to Discovery\n");

    SocketMessage m;
    m.type = 0x01;
    m.headerLength = strlen("NEW_BOWMAN");
    m.header = strdup("NEW_BOWMAN");
    m.data = strdup(bowman.username);

    sendSocketMessage(discoverySocketFD, m);

    printToConsole("Sent message to Discovery\n");
    // Receive response
    SocketMessage response = getSocketMessage(discoverySocketFD);

    // handle response
    connectToPoole(response);
    

    free(response.header);
    free(response.data);
    free(m.header);
    free(m.data);

    // ASK: IDK IF I SHOULD CLOSE THE SOCKET HERE
    close(discoverySocketFD);
    printToConsole("Disconnected from Discovery\n");

}

void listSongs(){
    printToConsole("LIST SONGS\n");
}
void checkDownloads(){
    printToConsole("CHECK DOWNLOADS\n");
}
void clearDownloads(){
    printToConsole("CLEAR DOWNLOADS\n");
}
void listPlaylists(){
    printToConsole("LIST PLAYLISTS\n");
}
void downloadFile(char *file){
    printToConsole("DOWNLOAD FILE\n");
    char *buffer;
    asprintf(&buffer, "Downloading file: %s\n", file);
    printToConsole(buffer);
    free(buffer);
}
void logout(){
    printToConsole("LOGOUT\n");
}
void freeUtilitiesBowman(){
    printToConsole("FREE UTILITIES BOWMAN\n");
}