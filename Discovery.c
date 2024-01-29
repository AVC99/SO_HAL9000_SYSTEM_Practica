#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>


#include "struct_definitions.h"
#include "io_utils.h"

int inputFileFd;
Discovery discovery;


void freeMemory(){
    free(discovery.pooleIP);
    free(discovery.bowmanIP);
}

void closeFds(){
    if (inputFileFd != 0){
        close(inputFileFd);
    }
}

void closeProgram(){
    freeMemory();
    closeFds();
    exit(0);
}

void saveDiscovery(char *filename){
    inputFileFd = open(filename, O_RDONLY);
    if (inputFileFd < 0){
        printError("Error: File not found\n");
        exit(1);
    }
    discovery.pooleIP = readUntil('\n', inputFileFd);
    discovery.pooleIP[strlen(discovery.pooleIP) - 1] = '\0';

    discovery.bowmanIP = readUntil('\n', inputFileFd);
    discovery.bowmanIP[strlen(discovery.bowmanIP) - 1] = '\0';

    char *port = readUntil('\n', inputFileFd);
    discovery.poolePort = atoi(port);
    free(port);

    port = readUntil('\n', inputFileFd);
    discovery.bowmanPort = atoi(port);
    free(port);
    close(inputFileFd);
}

int main(int argc, char *argv[]){
    signal(SIGINT, closeProgram);
    if (argc != 2){
        printError("Error: Wrong number of arguments\n");
        exit(1);
    }

    saveDiscovery(argv[1]);

    closeProgram();

    return 0;
}