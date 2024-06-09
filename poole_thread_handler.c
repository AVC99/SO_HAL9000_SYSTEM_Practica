#define _GNU_SOURCE
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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
 * @brief Lists the playlists in the Poole folder playlists are .txt files with songs in them
 * @param bowmanSocket The socket of the Bowman
 */
void listPlaylists(int bowmanSocket) {
    printToConsole("List Playlists\n");
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

        execlp("sh", "sh", "-c", "ls | grep .txt ", NULL);
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

        SocketMessage m;
        m.type = 0x02;
        m.headerLength = strlen("PLAYLISTS_RESPONSE");
        m.header = strdup("PLAYLISTS_RESPONSE");

        //* remaining size is the 256 - size of type, headerLength, header and \0
        size_t remainingBufferSize = BUFFER_SIZE - 3 - m.headerLength - 1;
        char *data = malloc(remainingBufferSize * sizeof(char));
        data[0] = '\0';

        int numBuffers = bytesRead / remainingBufferSize;
        if (bytesRead % remainingBufferSize != 0) {
            numBuffers++;  // add one extra buffer for the remaining bytes
        }

        char *printBuffer;
        asprintf(&printBuffer, "Pipebuffer : %s\n", pipeBuffer);
        printToConsole(printBuffer);
        free(printBuffer);

        const char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;

        if (chdir(folderPath) < 0) {
            printError("Error changing directory\n");
            exit(1);
        }

        char *fileName = strtok(pipeBuffer, "\n");
        asprintf(&printBuffer, "Filename: %s", fileName);
        printToConsole(printBuffer);
        free(printBuffer);

        size_t dataLength = 0;
        // FIRST BUFFER HAS F AND THE NUMBER OF BUFFERS
        // THE REST OF THE BUFFERS HAVE C AND THE NUMBER OF THE BUFFER
        char *numBuffersString;
        asprintf(&numBuffersString, "%d", numBuffers);
        strcat(data, "F");
        dataLength += strlen("F");
        strcat(data, numBuffersString);
        dataLength += strlen(numBuffersString);
        strcat(data, "&");
        dataLength += strlen("&");
        free(numBuffersString);
        numBuffersString = NULL;

        int buffersSent = 0;

        while (fileName != NULL) {
            int playlistfd = open(fileName, O_RDONLY);

            if (playlistfd < 0) {
                printError("Error: File not found\n");
                fileName = strtok(NULL, "\n");
                exit(1);
            }

            char *numOfSongs = readUntil('\n', playlistfd);
            int numOfSongsInt = atoi(numOfSongs);
            free(numOfSongs);
            strcat(data, fileName);
            dataLength += strlen(fileName);
            strcat(data, "&");
            dataLength += strlen("&");

            for (int i = 0; i < numOfSongsInt; i++) {
                char *songName = readUntil('\n', playlistfd);
                if (songName != NULL) {
                    strcat(data, songName);
                    dataLength += strlen(songName);
                    strcat(data, "&");
                    dataLength += strlen("&");
                    free(songName);
                }
            }
            close(playlistfd);
            strcat(data, "#");

            if (dataLength + 1 > remainingBufferSize) {  // +2 for the & and #
                m.data = strdup(data);
                //! REMOVE THIS
                char *buffer;
                asprintf(&buffer, "Sending message to Bowman: %s\n", m.data);
                printToConsole(buffer);
                free(buffer);
                sendSocketMessage(bowmanSocket, m);
                free(m.data);
                m.data = NULL;
                buffersSent++;
                data[0] = '\0';
                dataLength = 0;
                strcat(data, "C");
                dataLength += strlen("C");
                strcat(data, numBuffersString);
                dataLength += strlen(numBuffersString);
                free(numBuffersString);
                strcat(data, "&");
                dataLength += strlen("&");
            } else {
                strcat(data, "#");
                dataLength += strlen("#");
            }

            fileName = strtok(NULL, "\n");
        }

        if (strlen(data) > 0) {
            m.data = strdup(data);
            sendSocketMessage(bowmanSocket, m);
            free(m.data);
        }
        free(data);

        free(m.header);
        free(pipeBuffer);

        close(fd[0]);
        // change back to the original directory
        if (chdir("..") < 0) {
            printError("Error changing directory\n");
            exit(1);
        }
        waitpid(pid, NULL, 0);
    } else {
        printError("Error forking\n");
        exit(1);
    }
}
void sendFile(char *fileName, int bowmanSocket, int ID) {
    char *songPath;
    char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;
    asprintf(&songPath, "%s/%s", folderPath, fileName);
    int songfd = open(songPath, O_RDONLY | O_NONBLOCK);
    free(songPath);

    if (songfd < 0) {
        printError("Error: File not found\n");
        sendError(bowmanSocket);
        return;
    }
    char *idString;
    asprintf(&idString, "%d", ID);
    int idStringLength = strlen(idString);
    ssize_t bytesRead;
    // char *buffer;
    char c;
    char *data = malloc(FILE_MAX_DATA_SIZE );
    int dataLength = 0;

    for (int i = 0; i < idStringLength; i++) {
        data[dataLength] = idString[i];
        dataLength++;
    }
    data[dataLength] = '&';
    dataLength++;

    while ((bytesRead = read(songfd, &c, 1)) > 0) {
        if (bytesRead < 0) {
            printError("Error reading from file\n");
            sendError(bowmanSocket);
            return;
        }

        if ((size_t)dataLength < FILE_MAX_DATA_SIZE) {
            data[dataLength] = c;
            dataLength++;
        }

        if (dataLength == FILE_MAX_DATA_SIZE) {
            SocketMessage m;
            m.type = 0x04;
            m.headerLength = strlen("FILE_DATA");
            m.header = strdup("FILE_DATA");
            m.dataLength = dataLength;
            m.data = malloc(dataLength);
            memcpy(m.data, data, dataLength);
            sendSocketFile(bowmanSocket, m, dataLength);
            sleep(1);
            printToConsole("Sending START/MID message to Bowman\n");
            free(m.header);
            free(m.data);
            dataLength = 0;
            for (int i = 0; i < idStringLength; i++) {
                data[dataLength] = idString[i];
                dataLength++;
            }
            data[dataLength] = '&';
            dataLength++;
        }
    }
    if (dataLength > 0) {
        SocketMessage m;
        m.type = 0x04;
        m.headerLength = strlen("FILE_DATA");
        m.header = strdup("FILE_DATA");
        m.data = malloc(dataLength);
        m.dataLength = dataLength;
        memcpy(m.data, data, dataLength);
        sendSocketFile(bowmanSocket, m, dataLength);
        printToConsole("Sending END message to Bowman\n");
        free(m.header);
        free(m.data);
    }
    free(data);
    close(songfd);
}

/**
 * @brief Sends the the filename, the file size, the MD5 hash and an id to Bowman
 * @param songName The name of the song
 * @param bowmanSocket The socket of the Bowman
 */
int sendFileInfo(char *songName, int bowmanSocket) {
    struct stat st;
    char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;
    if (chdir(folderPath) < 0) {
        printError("Error changing directory\n");
        sendError(bowmanSocket);
        return -1;
    }
    if (stat(songName, &st) < 0) {
        printError("Error getting file info\n");
        sendError(bowmanSocket);
        return -1;
    }
    // get the md5 hash
    char *md5Hash = getMD5sum(songName);
    char *buffer;
    asprintf(&buffer, "md5sum %s", md5Hash);
    printToConsole(buffer);
    free(buffer);

    asprintf(&buffer, "File size: %ld\n", st.st_size);
    printToConsole(buffer);
    free(buffer);

    int randomId = getRand(0, 1000);

    SocketMessage m;
    m.type = 0x04;
    m.headerLength = strlen("NEW_FILE");
    m.header = strdup("NEW_FILE");
    char *data;
    asprintf(&data, "%s&%ld&%s&%d", songName, st.st_size, md5Hash, randomId);
    printToConsole(data);
    m.data = strdup(data);

    sendSocketMessage(bowmanSocket, m);

    free(m.header);
    free(m.data);

    if (chdir("..") < 0) {
        printError("Error changing back directory\n");
        sendError(bowmanSocket);
        return -1;
    }
    return randomId;
}

/**
 * @brief Sends the song data to the Bowman through the socket
 * @param songName The name of the song
 * @param bowmanSocket The socket of the Bowman
 */
void downloadSong(char *songName, int bowmanSocket) {
    printToConsole("Downloading song\n");
    int ID = sendFileInfo(songName, bowmanSocket);
    sleep(1);
    sendFile(songName, bowmanSocket, ID);
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
                listPlaylists(bowmanSocket);
                printToConsole("LIST PLAYLISTS\n");
            } else {
                printError("Error processing message from Bowman type 0x02\n");
                sendError(bowmanSocket);
            }
            break;
        }
        case 0x03: {
            if (strcmp(message.header, "DOWNLOAD_SONG") == 0) {
                downloadSong(message.data, bowmanSocket);
                printToConsole("DOWNLOAD SONG\n");

            } else if (strcmp(message.header, "DOWNLOAD_LIST") == 0) {
                // downloadPlaylist(message.data, bowmanSocket);
                printToConsole("DOWNLOAD PLAYLIST\n");

            } else {
                printError("Error processing message from Bowman type 0x03\n");
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
                printError("Error processing message from Bowman type 0x06\n");
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
            printError("Error processing message from Bowman delfault\n");
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
