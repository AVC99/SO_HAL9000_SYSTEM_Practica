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
#include "poole_thread_handler.h"
#include "semaphore_v2.h"
#include "struct_definitions.h"
#define MAX_THREADS 50

int terminate = FALSE, endMonolith = FALSE;
int inputFileFd, discoverySocketFD, bowmanSocketFD, numThreads = 0, bowmanSockets[MAX_THREADS];
Poole poole;
pthread_mutex_t terminateMutex = PTHREAD_MUTEX_INITIALIZER, numThreadsMutex = PTHREAD_MUTEX_INITIALIZER, pipeMutex;
pthread_t threads[MAX_THREADS];
semaphore writeMonolithSemaphore, syncMonolithSemaphore;
int monolithPipe[2];

/**
 * @brief Frees the memory allocated for the Poole struct
 */
void freeMemory() {
    free(poole.servername);
    free(poole.folder);
    free(poole.discoveryIP);
    free(poole.pooleIP);
}
/**
 * @brief Closes the file descriptors if they are open
 */
void closeFds() {
    if (inputFileFd != 0) {
        close(inputFileFd);
    }
    if (discoverySocketFD != 0) {
        close(discoverySocketFD);
    }
    if (bowmanSocketFD != 0) {
        close(bowmanSocketFD);
    }
}
/**
 * @brief Listens for stable bowman connections
 */
void listenForBowmans() {
    if ((bowmanSocketFD = createAndListenSocket(poole.pooleIP, poole.poolePort)) < 0) {
        printError("Error connecting to Bowman\n");
        exit(1);
    }

    pthread_mutex_lock(&terminateMutex);
    while (terminate == FALSE) {
        pthread_mutex_unlock(&terminateMutex);
        printToConsole("Listening for Bowmans...\n");

        int bowmanSocket = accept(bowmanSocketFD, NULL, NULL);

        if (bowmanSocket < 0) {
            printError("Error accepting Bowman\n");
            exit(1);
        }
        printToConsole("Bowman connected\n");

        // Allocate a new int on the heap and pass it tho the new thread
        SocketMessage m = getSocketMessage(bowmanSocket);
        printToConsole("Received message from Bowman\n");

        free(m.header);
        free(m.data);

        pthread_mutex_lock(&numThreadsMutex);
        if (numThreads < MAX_THREADS) {
            int *pBowmanSocket = malloc(sizeof(int));
            *pBowmanSocket = bowmanSocket;
            if (pthread_create(&threads[numThreads], NULL, bowmanThreadHandler, pBowmanSocket) != 0) {
                printError("Error creating thread\n");
                close(bowmanSocket);
                free(pBowmanSocket);
                exit(1);
            } else {
                bowmanSockets[numThreads] = bowmanSocket;
                numThreads++;
                //pthread_detach(threads[numThreads - 1]);
            }
        } else {
            printError("Error: Maximum number of threads reached\n");
        }
        pthread_mutex_unlock(&numThreadsMutex);
    }
    pthread_mutex_unlock(&terminateMutex);
    close(bowmanSocketFD);
}

/**
 * @brief Connects to the Discovery server with unstable connection
 * @param isExit If the program is exiting
 */
void connectToDiscovery(int isExit) {
    printToConsole("Connecting to Discovery\n");

    if ((discoverySocketFD = createAndConnectSocket(poole.discoveryIP, poole.discoveryPort)) < 0) {
        printError("Error connecting to Discovery\n");
        exit(1);
    }

    // CONNECTED TO DISCOVERY
    printToConsole("Connected to Discovery\n");

    if (isExit == FALSE) {
        SocketMessage m;
        m.type = 0x01;
        m.headerLength = strlen("NEW_POOLE");
        m.header = strdup("NEW_POOLE");
        char *data;

        asprintf(&data, "%s&%d&%s", poole.servername, poole.poolePort, poole.pooleIP);
        m.data = strdup(data);

        sendSocketMessage(discoverySocketFD, m);

        free(m.header);
        free(m.data);
        free(data);
    } else if (isExit == TRUE) {
        SocketMessage m;
        m.type = 0x06;
        m.headerLength = strlen("EXIT_POOLE");
        m.header = strdup("EXIT_POOLE");
        m.data = strdup("");

        sendSocketMessage(discoverySocketFD, m);

        free(m.header);
        free(m.data);
    }

    printToConsole("Sent message to Discovery\n");
    // Receive response
    SocketMessage response = getSocketMessage(discoverySocketFD);

    // handle response
    switch (response.type) {
        case 0x01:
            if (strcmp(response.header, "CON_OK") == 0) {
                printToConsole("Connection to Discovery successful\n");
                free(response.header);
                free(response.data);

                //* LISTEN FOR BOWMAN NEVER ENDS SO THE MEMORY IS NEVER FREED
                listenForBowmans();
            } else if (strcmp(response.header, "CON_KO") == 0) {
                printError("Connection to Discovery failed\n");
                free(response.header);
                free(response.data);
            }
            break;
        case 0x06:
            if (strcmp(response.header, "EXIT_OK") == 0) {
                printToConsole("Exit from Discovery successful\n");

            } else {
                printError("Exit from Discovery failed\n");
            }
            free(response.header);
            free(response.data);
            break;
        default:
            printError("Error: Wrong message type\n");
            free(response.header);
            free(response.data);
            break;
    }

    close(discoverySocketFD);
}

void notifyBowmans(){
    for (int i =0; i < numThreads; i++){
        SocketMessage m;
        m.type = 0x06;
        m.headerLength = strlen("EXIT_POOLE");
        m.header = strdup("EXIT_POOLE");
        m.data = strdup("");

        sendSocketMessage(bowmanSockets[i], m);

        free(m.header);
        free(m.data);
        pthread_join(threads[i], NULL);
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
    SEM_destructor(&writeMonolithSemaphore);
    SEM_destructor(&syncMonolithSemaphore);
    endMonolith = TRUE;
    close(monolithPipe[1]);

    notifyBowmans();

    connectToDiscovery(TRUE);
    printToConsole("Closing program\n");
    freeMemory();
    closeFds();
    exit(0);
}

/**
 * @brief Reads the file and saves the information in the Poole struct
 * @param filename The name of the file to read
 */
void savePoole(char *filename) {
    inputFileFd = open(filename, O_RDONLY);
    if (inputFileFd < 0) {
        printError("Error: File not found\n");
        exit(1);
    }
    poole.servername = readUntil('\n', inputFileFd);

    // check that the username does not contain &
    if (strchr(poole.servername, '&') != NULL) {
        printError("Error: Username contains &\n");
        char *newUsername = malloc((strlen(poole.servername) + 1) * sizeof(char));
        int j = 0;
        for (size_t i = 0; i < strlen(poole.servername); i++) {
            if (poole.servername[i] != '&') {
                newUsername[j] = poole.servername[i];
                j++;
            }
        }
        newUsername[j] = '\0';
        free(poole.servername);
        poole.servername = strdup(newUsername);
        free(newUsername);
    }

    poole.folder = readUntil('\n', inputFileFd);
    // poole.folder[strlen(poole.folder) - 1] = '\0';

    poole.discoveryIP = readUntil('\n', inputFileFd);
    // poole.discoveryIP[strlen(poole.discoveryIP) - 1] = '\0';

    char *firstPort = readUntil('\n', inputFileFd);
    poole.discoveryPort = atoi(firstPort);
    free(firstPort);

    poole.pooleIP = readUntil('\n', inputFileFd);
    // poole.pooleIP[strlen(poole.pooleIP) - 1] = '\0';

    char *secondPort = readUntil('\n', inputFileFd);
    poole.poolePort = atoi(secondPort);
    free(secondPort);
}
/**
 * @brief Shows the information read from the file
 */
void phaseOneTesting() {
    printToConsole("File read successfully\n");
    char *buffer;
    asprintf(&buffer, "Servername: %s\n", poole.servername);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Folder: %s\n", poole.folder);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Discovery IP: %s\n", poole.discoveryIP);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Discovery Port: %d\n", poole.discoveryPort);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Poole IP: %s\n", poole.pooleIP);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Poole Port: %d\n\n", poole.poolePort);
    printToConsole(buffer);
    free(buffer);
}
/**
 * @brief Initializes the semaphores
 */
void initializeSemaphore() {
    SEM_constructor_with_name(&writeMonolithSemaphore, ftok("Poole.c", 'a'));
    SEM_init(&writeMonolithSemaphore, 1);
    SEM_constructor(&syncMonolithSemaphore);
    SEM_init(&syncMonolithSemaphore, 0);
}
/**
 * @brief Starts the Monolith process
 */
void startMonolith() {
    initializeSemaphore();

    if (pipe(monolithPipe) == -1) {
        printError("Error creating pipe\n");
        exit(1);
    }

    pid_t pid = fork();
    if (pid == -1) {
        printError("Error creating monolith\n");
        exit(1);
    } else if (pid == 0) {
        // child
        close(monolithPipe[1]);

        while (endMonolith == FALSE) {
            SEM_wait(&syncMonolithSemaphore);
            SEM_wait(&writeMonolithSemaphore);
            printToConsole("Monolith received a song\n");
            sleep(1);

            int monolithFileFD = open("stats.txt", O_CREAT | O_RDWR, 0666);
            if (monolithFileFD < 0) {
                printError("Error creating stats file\n");
                exit(1);
            }

            char pipeBuffer[PIPE_BUFFER_SIZE] = {0};
            pthread_mutex_lock(&pipeMutex);
            ssize_t bytesRead = read(monolithPipe[0], pipeBuffer, PIPE_BUFFER_SIZE);
            pthread_mutex_unlock(&pipeMutex);
            

            if (bytesRead == -1) {
                printError("Error reading from pipe\n");
                exit(1);
            } else if (bytesRead > 0) {
                pipeBuffer[bytesRead] = '\0';
            } else if (bytesRead == 0) {
                printToConsole("0 bytes read\n");
            } else {
                printToConsole("IDK\n");
            }
            char *songName = strdup(pipeBuffer);
            char *buffer;

            struct stat fileStat;
            fstat(monolithFileFD, &fileStat);
            int fileSize = fileStat.st_size;
            
            if (fileSize == 0) {
               printToConsole("File is empty\n");
                asprintf(&buffer, "1\n%s&1\n", songName);
                write(monolithFileFD, buffer, strlen(buffer));
                free(buffer);
                close(monolithFileFD); 

            } else {
                printToConsole("File is not empty\n");
                lseek(monolithFileFD, 0, SEEK_SET);
                char *nOfSongsStr = readUntil('\n', monolithFileFD);
                int nOfSongs = atoi(nOfSongsStr);
                free(nOfSongsStr);
                int songFound = FALSE;
                char *newContent = malloc(fileSize + strlen(songName) + 10);
                newContent[0] = '\0';

                for (int i = 0; i< nOfSongs; i++){
                    char *song = readUntil('&', monolithFileFD);
                    char *songPlaysStr = readUntil('\n', monolithFileFD);
                    int songPlays = atoi(songPlaysStr);

                    if (strcmp(song, songName) == 0) {
                        songPlays++;
                        songFound = TRUE;
                        printToConsole("Song found\n");
                    }

                    asprintf(&buffer, "%s&%d\n", song, songPlays);
                    strcat(newContent, buffer);
                    free(buffer);
                    free(song);
                    free(songPlaysStr);
                }
                close(monolithFileFD);

                monolithFileFD = open("stats.txt", O_WRONLY | O_TRUNC, 0666);
                if (songFound == FALSE) {
                    printToConsole("Song not found\n");
                    asprintf(&buffer, "%s&1\n", songName);
                    strcat(newContent, buffer);
                    free(buffer);
                    nOfSongs++;
                }

                asprintf(&buffer, "%d\n", nOfSongs);
                write(monolithFileFD, buffer, strlen(buffer));
                free(buffer);
                write(monolithFileFD, newContent, strlen(newContent));
                free(newContent);
                close(monolithFileFD);
                
            }
            free(songName);
            SEM_signal(&writeMonolithSemaphore);
        }

        close(monolithPipe[0]);
        exit(0);

    } else {
        // parent
        close(monolithPipe[0]);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printError("Error: Missing arguments\n");
        return 1;
    }
    // reprogram SIGINT signal
    signal(SIGINT, closeProgram);
    pthread_mutex_init(&pipeMutex, NULL);

    savePoole(argv[1]);
    phaseOneTesting();
    // TODO : CREATE MONOLIT (FORK) PHASE 4
    startMonolith();

    // connect to Discovery
    connectToDiscovery(FALSE);

    closeProgram();
    return 0;
}