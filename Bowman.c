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
Bowman bowman;

void freeMemory() {
    // freeUtilitiesBowman();
    free(bowman.username);
    free(bowman.folder);
    free(bowman.ip);
}

void closeFds() {
    if (inputFileFd != 0) {
        close(inputFileFd);
    }
}

void closeProgram(int signal) {
    freeMemory();
    closeFds();
    exit(0);
}

void saveBowman(char* filename) {
    inputFileFd = open(filename, O_RDONLY);
    if (inputFileFd < 0) {
        printError("Error: File not found\n");
        exit(1);
    }

    bowman.username = readUntil('\n', inputFileFd);

    // check that the username does not contain &
    if (strchr(bowman.username, '&') != NULL) {
        printError("Error: Username contains &\n");
        char* newUsername = malloc((strlen(bowman.username) + 1) * sizeof(char));
        int j = 0;
        for (size_t i = 0; i < strlen(bowman.username); i++) {
            if (bowman.username[i] != '&') {
                newUsername[j] = bowman.username[i];
                j++;
            }
        }
        newUsername[j] = '\0';
        free(bowman.username);
        bowman.username = strdup(newUsername);
        free(newUsername);
    }
    bowman.folder = readUntil('\n', inputFileFd);
    bowman.ip = readUntil('\n', inputFileFd);
    char* port = readUntil('\n', inputFileFd);
    bowman.port = atoi(port);
    free(port);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printError("Error: Not enough arguments provided\n");
        return 1;
    }

    signal(SIGINT, closeProgram);

    saveBowman(argv[1]);

    closeProgram(0);
    return 0;
}