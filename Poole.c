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
#include "struct_definitions.h"
#include "helper.h"
#define MAX_THREADS 50

volatile int terminate = FALSE;
int inputFileFd, discoverySocketFD, bowmanSocketFD, numThreads = 0;
Poole poole;
pthread_mutex_t terminateMutex = PTHREAD_MUTEX_INITIALIZER, numThreadsMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_t threads[MAX_THREADS];

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
 * @brief Initialize the semaphore and shared memory for Monolithic
*/
void initStats() {
    int shmId = shmget(IPC_PRIVATE, sizeof(SharedStats), IPC_CREAT | 0666);
    if (shmId < 0) {
        perror("shmget");
        exit(1);
    }

    sharedStats = (SharedStats *)shmat(shmId, NULL, 0);
    if (sharedStats == (void *)-1) {
        perror("shmat");
        exit(1);
    }

    if (sem_init(&statsSem, 1, 1) < 0) {
        perror("sem_init");
        exit(1);
    }

    memset(sharedStats->downloadCounts, 0, sizeof(sharedStats->downloadCounts));
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
                numThreads++;
                pthread_detach(threads[numThreads - 1]);
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
    }

    printToConsole("Sent message to Discovery\n");
    // ! FIXME : WHEN CTRL+C IS PRESSED THIS HAS MEMORY LEAKS
    // Receive response
    SocketMessage response = getSocketMessage(discoverySocketFD);

    // handle response
    switch (response.type) {
        case 0x01:
            if (strcmp(response.header, "CON_OK") == 0) {
                printToConsole("Connection to Discovery successful\n");
                listenForBowmans();
            } else if (strcmp(response.header, "CON_KO") == 0) {
                printError("Connection to Discovery failed\n");
            }
            break;
        case 0x06:
            if (strcmp(response.header, "EXIT_OK") == 0) {
                printToConsole("Exit from Discovery successful\n");
            } else {
                printError("Exit from Discovery failed\n");
            }
            break;
        default:
            printError("Error: Wrong message type\n");
            break;
    }

    free(response.header);
    free(response.data);

    close(discoverySocketFD);
}

/**
 * @brief Closes the program correctly cleaning the memory and closing the file descriptors
 */
void closeProgram() {
    pthread_mutex_lock(&terminateMutex);
    terminate = TRUE;
    for(int i = 0; i < numThreads; i++){
        pthread_cancel(threads[i]);
    }
    pthread_mutex_unlock(&terminateMutex);

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
 * @brief Create and set monolithic process (Phase 4)
*/
void createMonolithProcess() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(1);
    } else if (pid == 0) {
        while (!terminate) {
            sem_wait(&statsSem);
            for (int i = 0; i < MAX_SONGS; i++) {
                printf("Song: %s, Downloads: %d\n", sharedStats->songNames[i], sharedStats->downloadCounts[i]);
            }
            sem_post(&statsSem);
            sleep(10);
        }
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printError("Error: Missing arguments\n");
        return 1;
    }
    // reprogram SIGINT signal
    signal(SIGINT, closeProgram);

    savePoole(argv[1]);
    phaseOneTesting();

    // connect to Discovery
    connectToDiscovery(FALSE);
    
    initStats();
    createMonolithProcess();
    closeProgram();
    return 0;
}