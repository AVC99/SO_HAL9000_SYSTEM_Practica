#include "helper.h"
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
SharedStats *sharedStats;
sem_t statsSem;
/**
 * @brief increment counter for every song download
*/
void incrementDownloadCount(const char *songName) {
    sem_wait(&statsSem);
    for (int i = 0; i < MAX_SONGS; i++) {
        if (strcmp(sharedStats->songNames[i], songName) == 0) {
            sharedStats->downloadCounts[i]++;
            break;
        }
    }
    sem_post(&statsSem);
}
