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
#include "semaphore_v2.h"
#include "struct_definitions.h"

extern pthread_mutex_t isPooleConnectedMutex, pipeMutex;
extern int terminate;
extern Poole poole;
extern int monolithPipe[2];
extern semaphore syncMonolithSemaphore;
extern pthread_mutex_t terminateMutex;
pthread_mutex_t socketMutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Lists the .mp3 songs in the Poole folder
 * @param bowmanSocket The socket of the Bowman
 */
void listSongs(int bowmanSocket) {
    printToConsole("Listing songs\n");
    int fd[2];
    if (pipe(fd)) {
        printError("Error creating pipe\n");
        return;
    }
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

        //!IDK WHAT NUMBER TO PUT IN  THE BUFFER SIZE
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

    

        const char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;

        if (chdir(folderPath) < 0) {
            printError("Error changing directory\n");
            exit(1);
        }

        char *fileName = strtok(pipeBuffer, "\n");
        
        size_t dataLength = 0;
        // FIRST BUFFER HAS F AND THE NUMBER OF BUFFERS
        // THE REST OF THE BUFFERS HAVE C AND THE NUMBER OF THE BUFFER
        char *numBuffersString;
        asprintf(&numBuffersString, "%d", numBuffers);
        strcat(data, numBuffersString);
        dataLength += strlen(numBuffersString);
        strcat(data, "#");
        dataLength += strlen("#");
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

/**
 * @brief Sends an error message to the Bowman
 * @param bowmanSocket The socket of the Bowman
 * @returns TRUE if the message was sent successfully, FALSE otherwise
 */
int sendFile(char *fileName, int bowmanSocket, int ID) {
    char *songPath;
    char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;
    asprintf(&songPath, "%s/%s", folderPath, fileName);
    int songfd = open(songPath, O_RDONLY | O_NONBLOCK);
    free(songPath);

    if (songfd < 0) {
        printError("Error: File not found\n");
        sendError(bowmanSocket);
        return FALSE;
    }
    char *idString;
    asprintf(&idString, "%d", ID);
    int idStringLength = strlen(idString);

    // char *buffer;
    char c;
    char *data = malloc(FILE_MAX_DATA_SIZE * sizeof(char));
    int dataLength = 0;

    for (int i = 0; i < idStringLength; i++) {
        data[dataLength++] = idString[i];
    }
    data[dataLength++] = '&';

    ssize_t bytesRead;
    while ((bytesRead = read(songfd, &c, 1)) > 0) {
        if (bytesRead < 0) {
            printError("Error reading from file\n");
            sendError(bowmanSocket);
            return FALSE;
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
            if(sendSocketFile(bowmanSocket, m, dataLength) == FALSE){
                free(m.header);
                free(m.data);
                free(data);
                free(idString);
                close(songfd);
                return FALSE;
            }
            
            sleep(1);
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
        if (sendSocketFile(bowmanSocket, m, dataLength) == FALSE) {
            free(m.header);
            free(m.data);
            free(data);
            free(idString);
            close(songfd);
            return FALSE;
        }
        
        free(m.header);
        free(m.data);
    }
    free(data);
    free(idString);
    close(songfd);
    return TRUE;
}

/**
 * @brief Sends the the filename, the file size, the MD5 hash and an id to Bowman
 * @param songName The name of the song
 * @param bowmanSocket The socket of the Bowman
 * @returns The random id sent to Bowman or -1 if an error occurred
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
    asprintf(&buffer, "md5sum %s\n", md5Hash);
    printToConsole(buffer);
    free(buffer);

    asprintf(&buffer, "File size: %ld\n", st.st_size);
    printToConsole(buffer);
    free(buffer);

    int randomId = getRand(1, 1000);  // random id between 1 and 1000 inclusive

    // SEND SIGNAL TO MONOLITH TO LOG THE DOWNLOAD
    SEM_signal(&syncMonolithSemaphore);
    printToConsole("Signaled Monolith\n");
    char *monolithBuffer = strdup(songName);

    pthread_mutex_lock(&pipeMutex);
    ssize_t bytesWritten = write(monolithPipe[1], songName, strlen(songName));
    pthread_mutex_unlock(&pipeMutex);

    free(monolithBuffer);
    if (bytesWritten < 0) {
        printError("Error writing to monolith pipe\n");
    }

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
    free(md5Hash);
    free(data);

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
void *downloadSong(void *arg) {
    DownloadThreadInfo *args = (DownloadThreadInfo *)arg;
    printToConsole("Downloading song\n");
    int ID = sendFileInfo(args->filename, args->socketFD);
    sleep(1);
    if (sendFile(args->filename, args->socketFD, ID) == FALSE) {
        printError("Error sending file\n");
        free(args->filename);
        free(args);
        close(args->socketFD);
        return NULL;
    }
    
    free(args->filename);
    free(args);
    return NULL;
}
/**
 * @brief Sends that a Bowman is exiting to Discovery and then back OK or KO to Bowman if Discovery has been able to remove the Bowman
 * @param data The data to send to Discovery (Bowman username)
 * @returns TRUE if the Bowman was removed from Discovery, FALSE otherwise
 */
int sendExitBowman(char *data) {
    printToConsole("Sending exit Bowman to Discovery\n");
    int discoverySFD;
    if ((discoverySFD = createAndConnectSocket(poole.discoveryIP, poole.discoveryPort, TRUE)) < 0) {
        printError("Error connecting to Discovery\n");
        return -1;
    }
    printToConsole("Connected to Discovery FOR BOWMAN EXIT\n");
    SocketMessage m;
    m.type = 0x06;
    m.headerLength = strlen("EXIT_BOWMAN");
    m.header = strdup("EXIT_BOWMAN");
    m.data = strdup(data);

    sendSocketMessage(discoverySFD, m);

    free(m.header);
    free(m.data);
    printToConsole("Sent message to Discovery\n");

    SocketMessage response = getSocketMessage(discoverySFD);
    if (strcmp(response.header, "CON_OK") == 0) {
        printToConsole("Received CON_OK from Discovery\n");

        close(discoverySFD);
        free(response.header);
        free(response.data);
        
        return TRUE;
    } else {
        printError("Error receiving CON_OK from Discovery\n");
        close(discoverySFD);
        free(response.header);
        free(response.data);
        return FALSE;
    }
}
/**
 * @brief Gets the file descriptor of the playlist
 * @param playlistName The name of the playlist
 * @returns The file descriptor of the playlist or -1 if an error occurred
 */
int getPlaylistFD(char *playlistName) {
    char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;
    char *playlistPath;
    asprintf(&playlistPath, "%s/%s", folderPath, playlistName);

    int playlistFd;
    if ((playlistFd = open(playlistPath, O_RDONLY)) < 0) {
        printError("Error opening playlist file\n");
        free(playlistPath);
        return -1;
    }
    free(playlistPath);

    return playlistFd;
}

/**
 * @brief Processes the message received from the Bowman and returns if the thread should terminate
 * @param message The message received from the Bowman
 * @param bowmanSocket The socket of the Bowman
 * @returns TRUE if the thread should terminate, FALSE otherwise
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
                pthread_t downloadSongThread;
                DownloadThreadInfo *downloadThreadInfo = malloc(sizeof(DownloadThreadInfo));
                if (downloadThreadInfo == NULL) {
                    printError("Error allocating memory for downloadThreadInfo\n");
                    return TRUE;
                }
                downloadThreadInfo->filename = strdup(message.data);
                downloadThreadInfo->socketFD = bowmanSocket;

                if (pthread_create(&downloadSongThread, NULL, downloadSong, (void *)downloadThreadInfo) != 0) {
                    printError("Error creating downloadSongThread\n");
                    return TRUE;
                } else {
                    pthread_detach(downloadSongThread);
                }
                // downloadSong(message.data, bowmanSocket);
                printToConsole("DOWNLOAD SONG\n");

            } else if (strcmp(message.header, "DOWNLOAD_LIST") == 0) {
                printToConsole("DOWNLOAD PLAYLIST\n");
                int playlistFd = getPlaylistFD(message.data);
                if (playlistFd == -1) {
                    printError("Error getting playlist file descriptor\n");
                } else {
                    char *numOfSongs = readUntil('\n', playlistFd);
                    int numOfSongsInt = atoi(numOfSongs);
                    free(numOfSongs);
                    //! TODO: MAKE THREAD ARRAY GLOBAL
                    pthread_t downloadSongThread[numOfSongsInt];

                    for (int i = 0; i < numOfSongsInt; i++) {
                        char *songName = readUntil('\n', playlistFd);
                        char *buffer;
                        asprintf(&buffer, "Song name: %s\n", songName);
                        printToConsole(buffer);
                        free(buffer);
                        if (songName != NULL) {
                            DownloadThreadInfo *downloadThreadInfo = malloc(sizeof(DownloadThreadInfo));
                            if (downloadThreadInfo == NULL) {
                                printError("Error allocating memory for downloadThreadInfo\n");
                            }
                            downloadThreadInfo->filename = strdup(songName);
                            downloadThreadInfo->socketFD = bowmanSocket;

                            if (pthread_create(&downloadSongThread[i], NULL, downloadSong, (void *)downloadThreadInfo) != 0) {
                                printError("Error creating downloadSongThread\n");
                            } else {
                                pthread_detach(downloadSongThread[i]);
                                sleep(1);
                            }
                            free(songName);
                        }
                    }
                    close(playlistFd);
                }

            } else {
                printError("Error processing message from Bowman type 0x03\n");
                sendError(bowmanSocket);
            }
            break;
        }
        case 0x05: {
            if (strcmp(message.header, "CHECK_OK") == 0) {
                printToConsole("MD5 OK\n");

            } else if (strcmp(message.header, "CHECK_KO") == 0) {
                printToConsole("MD5 KO\n");
            } else {
                printError("Error processing message from Bowman type 0x05\n");
                sendError(bowmanSocket);
            }

            break;
        }
        case 0x06: {
            if (strcmp(message.header, "EXIT") == 0) {
                printToConsole("Bowman disconnected\n");

                SocketMessage response;
                if (sendExitBowman(message.data) == TRUE) {
                    response.type = 0x06;
                    response.headerLength = strlen("CON_OK");
                    response.header = strdup("CON_OK");
                    response.data = strdup("");

                    sendSocketMessage(bowmanSocket, response);

                } else {
                    response.type = 0x06;
                    response.headerLength = strlen("CON_KO");
                    response.header = strdup("CON_KO");
                    response.data = strdup("");

                    sendSocketMessage(bowmanSocket, response);
                }

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
    SocketMessage message;

    pthread_mutex_lock(&terminateMutex);
    while (terminateThread == FALSE && terminate == FALSE) {
        pthread_mutex_unlock(&terminateMutex);
        printToConsole("Waiting for message from Bowman\n");
        message = getSocketMessage(bowmanSocket);
        printToConsole("Received message from Bowman\n");

        terminateThread = processBowmanMessage(message, bowmanSocket);

        if (message.data != NULL) {
            free(message.data);
            message.data = NULL;
        }
        if (message.header != NULL) {
            free(message.header);
            message.header = NULL;
        }
    }

    if (message.data != NULL) {
        free(message.data);
    }
    if (message.header != NULL) {
        free(message.header);
    }

    pthread_mutex_destroy(&socketMutex);
    close(bowmanSocket);
    return NULL;
}
