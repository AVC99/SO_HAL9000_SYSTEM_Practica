#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

// arnau.vives I3_6

void printToConsole(char *x);
void printError(char *x);
char *readUntil(char del, int fd);
void printArray(char *array);
char *get_md5sum(char *filename);
long long get_file_size(char *filename);
char *getMD5sum(char *fileName);
int getRand(int min, int max);
char *prependToBeginning(const char *original, const char *toPrepend);