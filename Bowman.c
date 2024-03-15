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
#include "bowman_utilities.h"

int inputFileFd;
Bowman bowman;
extern int discoverySocketFD, pooleSocketFD, isPooleConnected;
char *command;
void freeMemory() {
    // freeUtilitiesBowman();
    free(bowman.username);
    free(bowman.folder);
    free(bowman.ip);
    free(command);
}

void closeFds() {
    if (inputFileFd != 0) {
        close(inputFileFd);
    }
    if (discoverySocketFD != 0) {
        close(discoverySocketFD);
    }
    if(pooleSocketFD != 0){
        close(pooleSocketFD);
    }
}

void closeProgram() {
    logout();
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

void phaseOneTesting(){
    printToConsole("File read correctly\n");
    char *buffer;
    asprintf(&buffer, "Bowman username: %s\n", bowman.username);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Bowman folder: %s\n", bowman.folder);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Bowman IP: %s\n", bowman.ip);
    printToConsole(buffer);
    free(buffer);
    asprintf(&buffer, "Bowman port: %d\n", bowman.port);
    printToConsole(buffer);
    free(buffer);
    
}


/**
 * Reads the commands from the user and executes them until LOGOUT is called
 */
void commandInterpreter() {
    int bytesRead;
    
    int continueReading = TRUE;
    do {
        printToConsole("Bowman $ ");
        // READ THE COMMAND
        command = readUntil('\n', 0);
        bytesRead = strlen(command);
        if (bytesRead == 0) {
            break;
        }
        command[bytesRead] = '\0';

        // FORMAT THE COMMAND ADDING THE \0 AND REMOVING THE \n
        int len = strlen(command);
        if (len > 0 && command[len - 1] == '\n') {
            command[len - 1] = '\0';
        }

        // CHECK THE COMMAND can not use SWITCH because it does not work with strings :(
        if (strcasecmp(command, "CONNECT") == 0) {
            connectToDiscovery(FALSE);
            free(command);
        } else if (strcasecmp(command, "LOGOUT") == 0) {
            logout();
            continueReading = FALSE;
        } else  // IF THE COMMAND HAS MORE THAN ONE WORD
        {
            char *token = strtok(command, " ");

            if (token != NULL) {
                if (strcasecmp(token, "DOWNLOAD") == 0) {
                    char *filename = strtok(NULL, " ");
                    if (filename != NULL && strtok(NULL, " ") == NULL) {
                        downloadFile(filename);
                        free(command);
                    } else {
                        printError("Error: Missing arguments\n");
                        free(command);
                    }
                } else if (strcasecmp(token, "LIST") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL && strcasecmp(token, "SONGS") == 0) {
                        listSongs();
                        free(command);
                    } else if (token != NULL && strcasecmp(token, "PLAYLISTS") == 0) {
                        listPlaylists();
                        free(command);
                    } else {
                        printError("Error: Missing arguments\n");
                        free(command);
                    }
                } else if (strcasecmp(token, "CHECK") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL && strcasecmp(token, "DOWNLOADS") == 0) {
                        checkDownloads();
                        free(command);
                    } else {
                        printError("Error: Missing arguments\n");
                        free(command);
                    }
                } else if (strcasecmp(token, "CLEAR") == 0) {
                    token = strtok(NULL, " ");
                    if (token != NULL && strcasecmp(token, "DOWNLOADS") == 0) {
                        clearDownloads();
                        free(command);
                    } else {
                        printError("Error: Missing arguments\n");
                        free(command);
                    }
                } else {
                    printError("ERROR: Please input a valid command.\n");
                    free(command);
                }
            } else {
                printError("ERROR: Please input a valid command.\n");
                free(command);
            }
        }
    } while (continueReading == TRUE);
    // free(command); I FREE AT CLOSE PROGRAM
}


int main(int argc, char* argv[]) {
    if (argc < 2) {
        printError("Error: Not enough arguments provided\n");
        return 1;
    }

    signal(SIGINT, closeProgram);

    saveBowman(argv[1]);
    phaseOneTesting();

    commandInterpreter();

    closeProgram();
    return 0;
}