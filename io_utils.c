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

void printToConsole(char *x)
{
  write(1, x, strlen(x));
  fflush(stdout);
}

void printError(char *x)
{
  write(2, x, strlen(x));
  fflush(stderr);
}

char *readUntil(char del, int fd)
{

  char *chain = malloc(sizeof(char) * 1);
  char c;
  int i = 0, n;

  n = read(fd, &c, 1);

  while (c != del && n != 0 && n != -1)
  {
    chain[i] = c;
    i++;
    char *temp = realloc(chain, (sizeof(char) * (i + 1)));
    if (temp == NULL)
    {
      printError("Error reallocating memory\n");
      return NULL;
    }
    chain = temp;
    n = read(fd, &c, 1);
  }

  chain[i] = '\0';

  return chain;
}

void printArray(char *array)
{
  printToConsole("Printing array\n");
  char *buffer;
  for (size_t i = 0; i < strlen(array); i++)
  {
    asprintf(&buffer, "{%c}\n", array[i]);
    printToConsole(buffer);
    free(buffer);
  }
  printToConsole("Finished printing array\n");
}