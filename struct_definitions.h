#ifndef STRUCT_DEFINITIONS_H
#define STRUCT_DEFINITIONS_H

#include <stdint.h>

#define TRUE 1
#define FALSE 0
#define DOWNLOAD_SONG_PORT 8054
#define BUFFER_SIZE 256
#define MAX_BOWMANS 20
typedef struct
{
    uint8_t type;
    uint16_t headerLength;
    char *header;
    char *data;
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
    char *filename;
    int ID;
    long long fileSize;

} ThreadInfo;



#endif  // STRUCT_DEFINITIONS_H