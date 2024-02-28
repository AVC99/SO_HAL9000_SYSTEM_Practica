#ifndef HELPER_H
#define HELPER_H

#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#define MAX_SONGS 50
#define MAX_SONG_NAME_LENGTH 50

typedef struct {
    int downloadCounts[MAX_SONGS];
    char songNames[MAX_SONGS][MAX_SONG_NAME_LENGTH];
} SharedStats;

extern SharedStats *sharedStats;
extern sem_t statsSem;

#endif