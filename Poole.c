#define _GNU_SOURCE
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "io_utils.h"
#include "struct_definitions.h"
#include "network_utils.h"

int inputFileFd;
Poole poole;

void freeMemory() {
    free(poole.servername);
    free(poole.folder);
    free(poole.discoveryIP);
    free(poole.pooleIP);
}

void closeFds() {
    if (closeFds != 0) {
        close(inputFileFd);
    }
}

void closeProgram(int signal) {
    freeMemory();
    closeFds();
    exit(0);
}

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

void phaseOneTesting() {
    printToConsole("File read successfully\n") char *buffer;
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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printError("Error: Missing arguments\n");
        return 1;
    }

    // reprogram SIGINT signal
    signal(SIGINT, closeProgram);

    savePoole(argv[1]);
    phaseOneTesting();

    closeProgram(0);
    return 0;
}