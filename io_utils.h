#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

void printToConsole(char *x);
void printError(char *x);
char *readUntil(char del, int fd); 
void printArray(char *array);
char *get_md5sum(char *filename);
long long get_file_size(char *filename);
