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

// I don't know if we need a main file or not
int main (){

  write(1, "IDK if we need a main file\n", 28);
  return 0;
}