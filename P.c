#define _GNU_SOURCE
#define _POSIX_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <netdb.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

// arnau.vives joan.medina I3_6

Poole poole;
// FIXME: listenFD what is Bowman? Discovery?
int listenFD;
pthread_t *bowmanThreads, *downloadThreads, downloadThread;
int *bowmanClientSockets;
int bowmanThreadsCount = 0;
pthread_mutex_t bowmanThreadsMutex, bowmanClientSocketsMutex, downloadThreadsMutex, pooleMutex, downloadThreadMutex;

// FIXME: idk why this requires to comment the last char of the string and in Bowman and Discovery not
void savePoole(int fd) {
    poole.servername = readUntil('\n', fd);
    printArray(poole.servername);
    // poole.servername[strlen(poole.servername) - 1] = '\0';

    // check that the username does not contain &
    if (strchr(poole.servername, '&') != NULL) {
        printError("Error: Username contains &\n");
        char *newServername = malloc((strlen(poole.servername) + 1) * sizeof(char));
        int j = 0;
        for (size_t i = 0; i < strlen(poole.servername); i++) {
            if (poole.servername[i] != '&') {
                newServername[j] = poole.servername[i];
                j++;
            }
        }
        newServername[j] = '\0';
        free(poole.servername);
        poole.servername = strdup(newServername);
        free(newServername);
    }

    poole.folder = readUntil('\n', fd);
    // poole.folder[strlen(poole.folder) - 1] = '\0';

    poole.discoveryIP = readUntil('\n', fd);
    // poole.discoveryIP[strlen(poole.discoveryIP) - 1] = '\0';

    char *firstPort = readUntil('\n', fd);
    poole.discoveryPort = atoi(firstPort);
    free(firstPort);

    poole.pooleIP = readUntil('\n', fd);
    // poole.pooleIP[strlen(poole.pooleIP) - 1] = '\0';

    char *secondPort = readUntil('\n', fd);
    poole.poolePort = atoi(secondPort);
    free(secondPort);
}

void phaseOneTesting(Poole poole) {
    char *buffer;
    write(1, "File read correctly:\n", strlen("File read correctly:\n"));
    asprintf(&buffer, "Server - %s\n", poole.servername);
    write(1, buffer, strlen(buffer));
    free(buffer);
    asprintf(&buffer, "Folder - %s\n", poole.folder);
    write(1, buffer, strlen(buffer));
    free(buffer);
    asprintf(&buffer, "Discovery IP - %s\n", poole.discoveryIP);
    write(1, buffer, strlen(buffer));
    free(buffer);
    asprintf(&buffer, "Discovery Port - %d\n", poole.discoveryPort);
    write(1, buffer, strlen(buffer));
    free(buffer);
    asprintf(&buffer, "Poole IP - %s\n", poole.pooleIP);
    write(1, buffer, strlen(buffer));
    free(buffer);
    asprintf(&buffer, "Poole Port - %d\n", poole.poolePort);
    write(1, buffer, strlen(buffer));
    write(1, "\n", strlen("\n"));
    free(buffer);
}

void freeMemory() {
    free(poole.servername);
    free(poole.folder);
    free(poole.discoveryIP);
    free(poole.pooleIP);

    free(bowmanClientSockets);

    close(listenFD);
}

void closeThreads() {
    for (int i = 0; i < bowmanThreadsCount; i++) {
        // JOIN OR CANCEL THREADS?
        pthread_cancel(bowmanThreads[i]);
        // ptrhread_join(bowmanThreads[i], NULL);
    }
}

void destroyMutexes() {
    pthread_mutex_destroy(&bowmanThreadsMutex);
    pthread_mutex_destroy(&bowmanClientSocketsMutex);
}

void closeProgram() {
    freeMemory();
    closeThreads();
    destroyMutexes();
    exit(0);
}

void listSongs(int clientFD) {
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        printError("Error while creating pipe\n");
        exit(1);
    }

    pid_t pid = fork();

    if (pid < 0) {
        printError("Error while forking\n");
        exit(1);
    }
    if (pid == 0) {
        // CHILD
        char *buffer;
        asprintf(&buffer, "FOLDER %s\n", poole.folder);
        printToConsole(buffer);
        free(buffer);

        // REMOVE THE FIRST CHAR OF THE FOLDER
        const char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;

        asprintf(&buffer, "FOLDER %s\n", folderPath);
        printToConsole(buffer);
        free(buffer);

        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);  // Redirect stdout to write end of pipe
        close(pipefd[1]);                // Close write end of pipe now that it's been duplicated

        if (chdir(folderPath) != 0) {
            printError("Error while changing directory\n");
        }
        execlp("ls", "ls", NULL);

        // CODE SHOULD NOT REACH THIS POINT
        printError("Error while executing ls\n");
        exit(1);
    } else {
        // PARENT
        int status;
        waitpid(pid, &status, 0);

        close(pipefd[1]);

        // IDK WHAT NUMBER TO PUT HERE bcs this can be a lot of data
        char *pipeBuffer = malloc(200 * sizeof(char));
        ssize_t numRead = read(pipefd[0], pipeBuffer, 199);

        if (numRead == -1) {
            printError("Error while reading from pipe\n");
            exit(1);
        }

        pipeBuffer[numRead] = '\0';

        for (ssize_t i = 0; i < numRead; i++) {
            if (pipeBuffer[i] == '\n') {
                pipeBuffer[i] = '&';
            }
        }

        SocketMessage response;
        response.type = 0x02;
        response.headerLength = strlen("SONGS_RESPONSE");
        response.header = strdup("SONGS_RESPONSE");
        response.data = strdup(pipeBuffer);

        sendSocketMessage(clientFD, response);

        free(pipeBuffer);
        free(response.header);
        free(response.data);
    }
}

void *download_thread_handler(void *arg) {
    ThreadInfo threadInfo = *((ThreadInfo *)arg);

    // mutex
    pthread_mutex_lock(&pooleMutex);
    const char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;
    char *song_path = NULL;

    // asprintf(&song_path, "%s/%s", poole.folder, threadInfo.filename);
    asprintf(&song_path, "%s/%s", folderPath, threadInfo.filename);
    pthread_mutex_unlock(&pooleMutex);

    // dont need mutex for the file bcs it is only read
    int song_fd = open(song_path, O_RDONLY);
    if (song_fd < 0) {
        printError("Error while opening file\n");
        exit(1);
    }
    int remaining_buffer = BUFFER_SIZE - 1 - 2 - strlen("FILE_DATA") - 3 - 1;

    int number_of_messages = (int)((threadInfo.fileSize + remaining_buffer - 1) / remaining_buffer);
    printf("Number of messages: %d\n Remaining buffer = %d\n", number_of_messages, remaining_buffer);

    printToConsole("Creating socket to download song\n");
    // TOOD: MOVE THIS DOWN WHEN THE SOCKET WORKS
    close(song_fd);
    int downloadFD = createAndListenSocket(poole.pooleIP, DOWNLOAD_SONG_PORT);

    if (downloadFD < 0) {
        printError("Error while creating the socket\n");
        exit(1);
    }

    printToConsole("Waiting for Bowman to connect To downloads...\n");

    int bowman_download_fd = accept(downloadFD, (struct sockaddr *)NULL, NULL);

    if (bowman_download_fd < 0) {
        printError("Error while accepting\n");
        exit(1);
    }
    printToConsole("Bowman connected to downloads\n");

    /*char *buffer = malloc((remaining_buffer + 1) * sizeof(char));
    for (int i = 0; i < number_of_messages; i++)
    {

      ssize_t bytesRead = read(song_fd, buffer, remaining_buffer);

      if (bytesRead < 0)
      {
        printError("Error while reading from file\n");
        exit(1);
      }

      SocketMessage response;
      response.type = 0x04;
      response.headerLength = strlen("FILE_DATA");
      response.header = strdup("FILE_DATA");

      response.data = strndup(buffer, bytesRead);
      sendSocketMessage(bowman_download_fd, response);
      free(response.header);
      free(response.data);
    }*/

    free(song_path);

    close(downloadFD);
    close(bowman_download_fd);

    return NULL;
}

/**
 * Gets the file information and opens a thread to download it
 */
void download_song(char *song_name, int clientFD) {
    char *buffer;
    asprintf(&buffer, "DOWNLOAD_SONG %s\n", song_name);
    printToConsole(buffer);
    free(buffer);

    char *song_path;
    const char *folderPath = (poole.folder[0] == '/') ? (poole.folder + 1) : poole.folder;
    asprintf(&song_path, "%s/%s", folderPath, song_name);
    printToConsole(song_path);

    long long filesize = get_file_size(song_path);

    char *md5sum = get_md5sum(song_path);

    free(song_path);

    if (filesize < 0 || !md5sum) {
        printError("Error while getting file stats\n");
        sendError(clientFD);
    } else {
        printToConsole("File stats obtained\n");
        asprintf(&buffer, "File size: %lld \n", filesize);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "MD5SUM: %s \n", md5sum);
        printToConsole(buffer);
        free(buffer);

        // Check for asprintf errors
        char *header;
        if (asprintf(&header, "DOWNLOAD_SONG_RESPONSE") == -1) {
            printError("Error creating header\n");
            exit(1);
        }
        int id = rand() % 1000;
        char *data;
        if (asprintf(&data, "%s&%lld&%s&%i", song_name, filesize, md5sum, id) == -1) {
            printError("Error creating data\n");
            exit(1);
        }

        // Send initial response

        SocketMessage response;
        response.type = 0x03;
        response.headerLength = strlen(header);
        response.header = strdup(header);
        response.data = strdup(data);

        sendSocketMessage(clientFD, response);

        free(header);
        free(data);
        free(md5sum);

        free(response.header);
        free(response.data);

        // Open a thread to download the file
        ThreadInfo threadInfo;
        threadInfo.socketFD = clientFD;
        threadInfo.filename = strdup(song_name);
        threadInfo.fileSize = filesize;
        threadInfo.ID = id;

        if (pthread_create(&downloadThread, NULL, download_thread_handler, &threadInfo) != 0) {
            printError("Error creating download thread\n");
            exit(1);
        }
    }
}

int proccessBowmanMessage(SocketMessage message, int clientFD) {
    switch (message.type) {
        case 0x02:
            if (strcmp(message.header, "LIST_SONGS") == 0) {
                listSongs(clientFD);

                free(message.header);
                free(message.data);
            } else if (strcmp(message.header, "LIST_PLAYLISTS") == 0) {
                // TODO: REPEAT THE SAME PROCESS AS LIST_SONGS BUT IDK WHAT IS A PLAYLIST IN THE CMD
                SocketMessage response;
                response.type = 0x02;
                response.headerLength = strlen("PLAYLIST_RESPONSE");
                response.header = strdup("PLAYLIST_RESPONSE");
                response.data = strdup("playlist1&playlist2&playlist3");

                sendSocketMessage(clientFD, response);

                free(message.header);
                free(message.data);
            } else {
                printError("ERROR while connecting to Bowman\n");

                sendError(clientFD);
            }
            break;

        case 0x03: {
            if (strcmp(message.header, DOWNLOAD_SONG) == 0) {
                download_song(message.data, clientFD);

                free(message.header);
                free(message.data);
            } else if (strcmp(message.header, "DOWNLOAD_PLAYLIST") == 0) {
                char *buffer;
                asprintf(&buffer, "DOWNLOAD_PLAYLIST %s\n", message.data);
                printToConsole(buffer);
                free(buffer);

                SocketMessage response;
                response.type = 0x03;
                response.headerLength = strlen("DOWNLOAD_PLAYLIST_RESPONSE");
                response.header = strdup("DOWNLOAD_PLAYLIST_RESPONSE");
                response.data = strdup("playlist1data");

                sendSocketMessage(clientFD, response);

                free(message.header);
                free(message.data);
            } else {
                sendError(clientFD);
                printError("ERROR while connecting to Bowman\n");
            }
            break;
        }

        case 0x06:
            if (strcmp(message.header, "EXIT") == 0) {
                printToConsole("EXIT DETECTED\n");

                // I GUESS I HAVE TO SEND A MESSAGE TO DISCOVERY TO REMOVE THE BOWMAN

                // MESSAGE FOR BOWMAN
                SocketMessage response;

                response.type = 0x06;
                response.headerLength = strlen("CON_OK");
                response.header = strdup("CON_OK");
                response.data = strdup("");

                sendSocketMessage(clientFD, response);

                printToConsole("Bowman disconnected msg sent\n");
                free(response.header);
                free(response.data);

                // FIXME: MESSAGE FOR DISCOVERY
                bzero(&response, sizeof(response));

                SocketMessage response2;
                printToConsole("Sending message to Discovery\n");
                response2.type = 0x02;
                response2.headerLength = strlen("REMOVE_BOWMAN");
                response2.header = strdup("REMOVE_BOWMAN");

                /*int dataLength = strlen(message.data);
                response.data[dataLength]='\0';*/

                response2.data = strdup(message.data);

                int discoveryFD;
                if ((discoveryFD = createAndConnectSocket(poole.discoveryIP, poole.discoveryPort)) < 0) {
                    printError("Error creating the socket\n");
                    exit(1);
                }

                sendSocketMessage(discoveryFD, response2);

                printToConsole("Bowman disconnected msg sent to Discovery\n");

                printToConsole("Bowman disconnected\n");

                free(response.header);
                free(response.data);

                free(message.header);
                free(message.data);

                return TRUE;
            }

            break;

        default:
            printError("IDK WHAT MESSAGE IS THIS\n");
            sendError(clientFD);
            return TRUE;
            break;
    }

    return FALSE;
}

void *bowmanThreadHandler(void *arg) {
    int bowmanFD = *((int *)arg);
    free(arg);

    int exit = FALSE;
    SocketMessage message;

    printToConsole("Bowman thread created\n");
    printToConsole("Listening for thread bowman messages...\n");

    while (exit == FALSE) {
        // bzero(&message, sizeof(message));

        message = getSocketMessage(bowmanFD);
        printToConsole("Bowman message received\n");

        exit = proccessBowmanMessage(message, bowmanFD);
        bzero(&message, sizeof(message));

        free(message.header);
        free(message.data);
    }

    return NULL;
}

void listenForBowmans() {
    int listenBowmanFD;

    if ((listenBowmanFD = createAndListenSocket(poole.pooleIP, poole.poolePort)) < 0) {
        printError("Error creating the socket\n");
        exit(1);
    }

    while (1) {
        printToConsole("\nWaiting for Bowman connections...\n");

        int clientFD = accept(listenBowmanFD, (struct sockaddr *)NULL, NULL);

        if (clientFD < 0) {
            printError("Error while accepting\n");
            exit(1);
        }

        printToConsole("Bowman connected\n");

        SocketMessage message = getSocketMessage(clientFD);

        switch (message.type) {
            case 0x01:
                if (strcmp(message.header, "NEW_BOWMAN") == 0) {
                    printToConsole("NEW_BOWMAN DETECTED\n");

                    SocketMessage response;
                    response.type = 0x01;
                    response.headerLength = strlen("CON_OK");
                    response.header = strdup("CON_OK");
                    response.data = strdup("");

                    sendSocketMessage(clientFD, response);
                    char *buffer;
                    asprintf(&buffer, "Bowman clientFD: %d\n", clientFD);
                    printToConsole(buffer);
                    free(buffer);

                    //  Open a thread for the bowman
                    pthread_t bowmanThread;
                    int *FDPointer = malloc(sizeof(int));
                    *FDPointer = clientFD;

                    if (pthread_create(&bowmanThread, NULL, bowmanThreadHandler, FDPointer) != 0) {
                        printError("Error creating bowman thread\n");
                        close(clientFD);
                        exit(1);
                    }

                    bowmanThreadsCount++;
                    pthread_mutex_lock(&bowmanThreadsMutex);
                    bowmanThreads = realloc(bowmanThreads, bowmanThreadsCount * sizeof(pthread_t));
                    bowmanThreads[bowmanThreadsCount - 1] = bowmanThread;
                    pthread_mutex_unlock(&bowmanThreadsMutex);
                    pthread_mutex_lock(&bowmanClientSocketsMutex);
                    bowmanClientSockets = realloc(bowmanClientSockets, bowmanThreadsCount * sizeof(int));
                    bowmanClientSockets[bowmanThreadsCount - 1] = listenBowmanFD;
                    pthread_mutex_unlock(&bowmanClientSocketsMutex);

                    free(message.header);
                    free(message.data);
                } else {
                    printError("ERROR while connecting to Bowman\n");

                    SocketMessage response;
                    response.type = 0x01;
                    response.headerLength = strlen("CON_KO");
                    response.header = strdup("CON_KO");
                    response.data = strdup("");

                    sendSocketMessage(clientFD, response);

                    free(message.header);
                    free(message.data);
                }
                break;

            default:
                // TODO: HANDLE DEFAULT
                break;
        }
    }
}

void connectToDiscovery() {
    // TODO: REFACTOR THIS TO NEW METHODS

    int socketFD;

    if ((socketFD = createAndConnectSocket(poole.discoveryIP, poole.discoveryPort)) < 0) {
        printError("Error creating the socket\n");
        exit(1);
    }

    // CONNECTED TO DISCOVERY
    printToConsole("Connected to Discovery\n");

    SocketMessage sending = {
        .type = 0x01,
        .headerLength = strlen("NEW_POOLE"),
        .header = strdup("NEW_POOLE"),
    };

    // MARK --------------------------------------------------------------------------------------
    //  IDK WHY IF I PUT THE PORT LAST IT DOESNT WORK
    char *data;
    asprintf(&data, "%s&%d&%s", poole.servername, poole.poolePort, poole.pooleIP);

    sending.data = strdup(data);
    free(data);

    sendSocketMessage(socketFD, sending);

    // RECEIVE MESSAGE
    SocketMessage message = getSocketMessage(socketFD);

    switch (message.type) {
        case 0x01:
            if (strcmp(message.header, "CON_OK") == 0) {
                listenForBowmans();
            } else if (strcmp(message.header, "CON_KO") == 0) {
                printError("ERROR while connecting to Discovery\n");
            }
            break;

        default:
            break;
    }
    printToConsole("Discovery message received\n");

    free(message.header);
    free(message.data);

    free(sending.header);
    free(sending.data);

    close(socketFD);
}

int main(int argc, char *argv[]) {
    // Reprogram the SIGINT signal
    signal(SIGINT, closeProgram);

    // Check if the arguments are provided
    if (argc < 2) {
        write(2, "Error: Missing arguments\n", strlen("Error: Missing arguments\n"));
        return 1;
    }

    // Check if the file exists and can be opened
    int fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        write(2, "Error: File not found\n", strlen("Error: File not found\n"));
        return 1;
    }

    // Save the poole information
    savePoole(fd);
    close(fd);

    // THIS IS FOR PHASE 1 TESTING
    phaseOneTesting(poole);

    // initialize mutexes
    pthread_mutex_init(&bowmanThreadsMutex, NULL);
    pthread_mutex_init(&bowmanClientSocketsMutex, NULL);
    pthread_mutex_init(&downloadThreadsMutex, NULL);
    pthread_mutex_init(&pooleMutex, NULL);
    pthread_mutex_init(&downloadThreadMutex, NULL);

    connectToDiscovery();

    for (int i = 0; i < bowmanThreadsCount; i++) {
        pthread_join(bowmanThreads[i], NULL);
    }

    closeProgram();

    return 0;
}