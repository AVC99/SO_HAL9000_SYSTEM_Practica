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

char *get_md5sum(char *filename)
{
  int pipefd[2];
  if (pipe(pipefd) == -1)
  {
    printError("Error while creating pipe\n");
    exit(1);
  }

  pid_t pid = fork();
  int status;

  switch (pid)
  {
  case -1:
    // ERROR
    printError("Error while forking\n");
    exit(1);
    break;

  case 0:
    // CHILD
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to write end of pipe
    execlp("md5sum", "md5sum", filename, (char *)NULL);
    break;

  default:
    // PARENT
    // wait for the child to finish
    
    waitpid(pid, &status, 0);
    close(pipefd[1]);

    char *md5sum = malloc(33 * sizeof(char));
    if (!md5sum)
    {
      printError("Error while allocating memory\n");
      exit(1);
    }
    size_t numRead = read(pipefd[0], md5sum, 32);
    if (numRead == (size_t) -1)
    {
      printError("Error while reading from pipe\n");
      exit(1);
    }
    md5sum[32] = '\0';
    close(pipefd[0]);

    return md5sum;

    break;
  }
  return NULL;
}
long long get_file_size(char *filename)
{
  struct stat fileStat;

  if (stat(filename, &fileStat) < 0)
  {
    printError("Error while getting file stats\n");
    return -1;
  }
  return (long long)fileStat.st_size;
}