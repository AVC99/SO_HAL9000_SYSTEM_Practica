#ifndef STRUCT_DEFINITIONS_H
#define STRUCT_DEFINITIONS_H

#include <stdint.h>

// arnau.vives I3_6

#define TRUE 1
#define FALSE 0
#define DOWNLOAD_SONG_PORT 8054
#define BUFFER_SIZE 256
#define MAX_BOWMANS 20
#define PIPE_BUFFER_SIZE 256
#define FILE_MAX_DATA_SIZE (BUFFER_SIZE - 3 - strlen("FILE_DATA")) // 256 - 3 - 9 = 244
typedef struct
{
    uint8_t type;
    uint16_t headerLength;
    char *header;
    char *data;
    int dataLength;
} SocketMessage;

typedef struct
{
    char *username;
    char *folder;
    char *ip;
    int port;
} Bowman;

typedef struct
{
    char *servername;
    char *folder;
    char *discoveryIP;
    int discoveryPort;
    char *pooleIP;
    int poolePort;
} Poole;

typedef struct
{
    char *pooleIP;
    int poolePort;
    char *bowmanIP;
    int bowmanPort;
} Discovery;

typedef struct
{
    int numOfBowmans;
    int poolePort;
    char *pooleIP;
    char *pooleServername;
    char *bowmans[MAX_BOWMANS];
} PooleServer;

typedef struct {
    int socketFD;
    //socket no fa falta
    //TODO: REMOVE socketFD
    char *filename;
    int ID;
    long long fileSize;

} ThreadInfo;

typedef struct{
    int socketFD;
    char* filename;
}DownloadThreadInfo;
typedef struct {
    long ID;
    int rawDataSize;
    char data[BUFFER_SIZE - 3 - 9];
} queueMessage;

typedef struct{
    int downloadedChunks;
    int totalChunks;
    char* filename;
} ChunkInfo;

#endif  // STRUCT_DEFINITIONS_H