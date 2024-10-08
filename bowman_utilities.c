#define _GNU_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <dirent.h>
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

// I3_G6 arnau.vives

extern Bowman bowman;
int discoverySocketFD, pooleSocketFD, isPooleConnected = FALSE;
pthread_mutex_t isPooleConnectedMutex;
extern pthread_t listenThread;

extern ChunkInfo *chunkInfo;
extern pthread_mutex_t chunkInfoMutex;
extern int nOfDownloadingSongs;

/**
 * @brief Connects to the Poole server with stable connection
 * @param response message received from the Discovery server
 */
void connectToPoole(SocketMessage response) {

    char *dataCopy = strdup(response.data);
    char *pooleServename = strtok(dataCopy, "&");
    char *poolePort = strtok(NULL, "&");
    char *pooleIP = strtok(NULL, "&");
    
    char *buffer;
    asprintf(&buffer, "%s connected to HAL 9000 system (%s), welcome music lover!!\n", bowman.username, pooleServename);
    printToConsole(buffer);
    free(buffer);
    

    //printToConsole("Poole port: ");
    //printToConsole(poolePort);
    //printToConsole("\n");

    //printToConsole("Poole IP: ");
    //printToConsole(pooleIP);
    //printToConsole("\n");

    int poolePortInt = atoi(poolePort);

    // CONNECT TO POOLE
    if ((pooleSocketFD = createAndConnectSocket(pooleIP, poolePortInt, FALSE)) < 0) {
        printError("Error connecting to Poole\n");
        exit(1);
    }

    free(dataCopy);
    isPooleConnected = FALSE;
    pthread_mutex_init(&isPooleConnectedMutex, NULL);
    // Send message to Poole
    SocketMessage m;
    m.type = 0x01;
    m.headerLength = strlen("NEW_BOWMAN");
    m.header = strdup("NEW_BOWMAN");
    m.data = strdup(bowman.username);

    sendSocketMessage(pooleSocketFD, m);

    free(m.header);
    free(m.data);


    SocketMessage pooleResponse = getSocketMessage(pooleSocketFD);


    if (pooleResponse.type == 0x01 && strcmp(pooleResponse.header, "CON_OK") == 0) {

        pthread_mutex_lock(&isPooleConnectedMutex);
        isPooleConnected = TRUE;
        pthread_mutex_unlock(&isPooleConnectedMutex);

        // Start listening to Poole no need to pass the socket file descriptor to the thread
        // because it is a extern global variable
        if (pthread_create(&listenThread, NULL, listenToPoole, NULL) != 0) {
            printError("Error creating thread\n");
            close(pooleSocketFD);
            exit(1);
        }

        // pthread_detach(listenThread);

    } else {
        printError("Error connecting to Poole\n");
        exit(1);
    }
    free(pooleResponse.header);
    free(pooleResponse.data);

    printToConsole("Bowman $ ");

    //! IMPORTANT: response is freed in the main function
}
/**
 * @brief If the Bowman is connected to the Poole server, it disconnects from it and sends a message to the Discovery server
 */
void logout() {
    pthread_mutex_lock(&isPooleConnectedMutex);
    if (isPooleConnected == TRUE) {
        SocketMessage m;
        m.type = 0x06;
        m.headerLength = strlen("EXIT");
        m.header = strdup("EXIT");
        m.data = strdup(bowman.username);

        sendSocketMessage(pooleSocketFD, m);

        free(m.header);
        free(m.data);

        sleep(1);
        isPooleConnected = FALSE;
        pthread_mutex_unlock(&isPooleConnectedMutex);

        pthread_join(listenThread, NULL);
        printToConsole("Thread joined\n");
        close(pooleSocketFD);
        return;
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);
}

/**
 * @brief Connects to the Discovery server with unstable connection
 * @param isExit to know if the Bowman is exiting (need to send disconnect to Discovery) or not (need to connect to Poole)
 */
int connectToDiscovery(int isExit) {
    if ((discoverySocketFD = createAndConnectSocket(bowman.ip, bowman.port, FALSE)) < 0) {
        printError("Error connecting to Discovery\n");
        exit(1);
    }

    // CONNECTED TO DISCOVERY

    SocketMessage m;
    if (isExit == FALSE) {
        m.type = 0x01;
        m.headerLength = strlen("NEW_BOWMAN");
        m.header = strdup("NEW_BOWMAN");
        m.data = strdup(bowman.username);
        sendSocketMessage(discoverySocketFD, m);
        free(m.header);
        free(m.data);
    } else if (isExit == TRUE) {
        m.type = 0x06;
        m.headerLength = strlen("EXIT");
        m.header = strdup("EXIT");
        m.data = strdup(bowman.username);
        sendSocketMessage(discoverySocketFD, m);
    }

    // Receive response
    SocketMessage response = getSocketMessage(discoverySocketFD);

    // handle response
    if (isExit == FALSE) {
        if (response.type == 0x01 && strcmp(response.header, "CON_OK") == 0) {
            connectToPoole(response);
        } else if (response.type == 0x01 && strcmp(response.header, "CON_KO") == 0) {
            printError("Error connecting to Discovery NO POOLES\n");
        }

        free(response.header);
        free(response.data);
        close(discoverySocketFD);

        return FALSE;
    } else if (isExit == TRUE) {
        if (response.type == 0x06 && strcmp(response.header, "CON_OK") == 0) {
            printToConsole("Bowman disconnected from Discovery\n");
        } else if (response.type == 0x06 && strcmp(response.header, "CON_KO") == 0) {
            printError("Error disconnecting from Discovery\n");
            logout();
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

}
/**
 * @brief Shows the .txt files in the folder
 * @param folderPath path of the folder where the playlists are downloaded without the first slash
 */
void checkDownloadedPlaylists(const char *folderPath) {
    DIR *dir = opendir(folderPath);
    if (dir == NULL) {
        printError("Error while opening directory\n");
        return;
    }

    char *buffer;
    asprintf(&buffer, "Playlists in the %s folder :\n", folderPath);
    printToConsole(buffer);
    free(buffer);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext != NULL && strcmp(ext, ".txt") == 0) {
            char *filePath;
            asprintf(&filePath, "%s/%s", folderPath, entry->d_name);

            asprintf(&buffer, "Playlist : %s\n", entry->d_name);
            printToConsole(buffer);
            free(buffer);

            int fd = open(filePath, O_RDONLY);
            if (fd < 0) {
                printError("Error opening file\n");
                continue;
            } else {
                char *numSongsStr = readUntil('\n', fd);
                int numSongs = atoi(numSongsStr);
                free(numSongsStr);
                for (int i = 0; i < numSongs; i++) {
                    char *songName = readUntil('\n', fd);
                    asprintf(&buffer, "\t- %s\n", songName);
                    printToConsole(buffer);
                    free(buffer);
                    free(songName);
                }
            }

            free(filePath);
            close(fd);
        }
    }
    printToConsole("\n");
    closedir(dir);
}

/**
 * @brief Shows the .mp3 files in the folder
 * @param folderPath path of the folder where the songs are downloaded without the first slash
 */
void checkDownloadedSongs(const char *folderPath) {
    DIR *dir = opendir(folderPath);
    if (dir == NULL) {
        printError("Error while opening directory\n");
        return;
    }

    char *buffer;
    asprintf(&buffer, "\nSongs in the %s folder :\n", folderPath);
    printToConsole(buffer);
    free(buffer);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        char *ext = strrchr(entry->d_name, '.');
        if (ext != NULL && strcmp(ext, ".mp3") == 0) {
            asprintf(&buffer, "\t- %s\n", entry->d_name);
            printToConsole(buffer);
            free(buffer);
        }
    }
    closedir(dir);
}

/**
 * @brief Shows the songs and playlists downloaded in the Bowman console
 */
void checkDownloads() {
    const char *folderPath = (bowman.folder[0] == '/') ? (bowman.folder + 1) : bowman.folder;
    char *buffer;

    pthread_mutex_lock(&chunkInfoMutex);
    for (int i = 0; i < nOfDownloadingSongs; i++) {
        asprintf(&buffer, "Downloading song: %s\n", chunkInfo[i].filename);
        printToConsole(buffer);
        free(buffer);

        float percentage = (float)chunkInfo[i].downloadedChunks / chunkInfo[i].totalChunks * 100;
        int barLength = 20;
        int progress = (int)(percentage / 100 * barLength);
        char progressBar[barLength + 1];
        for (int j = 0; j < barLength; j++) {
            if (j < progress)
                progressBar[j] = '=';
            else
                progressBar[j] = ' ';
        }
        progressBar[barLength] = '\0';
        asprintf(&buffer, "Progress: [%s] %.2f%% (%d/%d)\n", progressBar, percentage, chunkInfo[i].downloadedChunks, chunkInfo[i].totalChunks);
        printToConsole(buffer);
        free(buffer);
    }
    pthread_mutex_unlock(&chunkInfoMutex);

    checkDownloadedSongs(folderPath);

    // checkDownloadedPlaylists(folderPath);
    printToConsole("Bowman $ ");
}

/**
 * @brief Clears everything in Bowman the folder
 */
void clearDownloads() {
    printToConsole("Clearing downloads in the folder...\n");

    char cwd[1000];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        printError("Error while getting current directory\n");
        return;
    }
    // remove the first slash if it exists
    const char *folderPath = (bowman.folder[0] == '/') ? (bowman.folder + 1) : bowman.folder;

    // Change to the directory
    if (chdir(folderPath) != 0) {
        printError("Error while changing directory\n");
        return;
    }

    // Remove all files in the directory
    int status = system("rm -v *");
    if (status == -1) {
        printError("Error while removing files\n");
    }

    // Change back to the original directory
    if (chdir(cwd) != 0) {
        printError("Error while changing back to original directory\n");
    }
    printToConsole("Downloads cleared\nBowman $ ");
}
/**
 * @brief Sends LIST_PLAYLISTS message to Poole
 */
void listPlaylists() {
    pthread_mutex_lock(&isPooleConnectedMutex);
    if (isPooleConnected == FALSE) {
        pthread_mutex_unlock(&isPooleConnectedMutex);
        printError("You are not connected to Poole\n");
        return;
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);

    SocketMessage m;

    m.type = 0x02;
    m.headerLength = strlen("LIST_PLAYLISTS");
    m.header = strdup("LIST_PLAYLISTS");
    m.data = strdup("");

    sendSocketMessage(pooleSocketFD, m);

    free(m.header);
    free(m.data);
}
/**
 * @brief Sends the message to Poole to download a file
 * @param file name of the file to download
 */
void downloadFile(char *file) {
    pthread_mutex_lock(&isPooleConnectedMutex);
    if (isPooleConnected == FALSE) {
        pthread_mutex_unlock(&isPooleConnectedMutex);
        printError("You are not connected to Poole\n");
        return;
    }
    pthread_mutex_unlock(&isPooleConnectedMutex);

    char *buffer;
    asprintf(&buffer, "Downloading file: %s\n", file);
    printToConsole(buffer);
    free(buffer);

    char *extension = strrchr(file, '.');
    if (extension != NULL) {
        if (strcmp(extension, ".mp3") == 0) {
            printToConsole("Downloading song\n");

            SocketMessage m;

            m.type = 0x03;
            m.headerLength = strlen("DOWNLOAD_SONG");
            m.header = strdup("DOWNLOAD_SONG");
            m.data = strdup(file);

            sendSocketMessage(pooleSocketFD, m);

            free(m.header);
            free(m.data);

        } else if (strcmp(extension, ".txt") == 0) {
            asprintf(&buffer, "Downloading playlist: %s\n", file);
            printToConsole(buffer);
            free(buffer);

            SocketMessage m;

            m.type = 0x03;
            m.headerLength = strlen("DOWNLOAD_LIST");
            m.header = strdup("DOWNLOAD_LIST");
            m.data = strdup(file);

            sendSocketMessage(pooleSocketFD, m);

            free(m.header);
            free(m.data);

        } else {
            printError("Unknown file type. Only .mp3 for songs and .txt for playlists\n");
        }
    } else {
        printError("File has no extension\n");
    }
}
