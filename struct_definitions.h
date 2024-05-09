#ifndef STRUCT_DEFINITIONS_H
#define STRUCT_DEFINITIONS_H

#include <stdint.h>

#define TRUE 1
#define FALSE 0
#define DOWNLOAD_SONG_PORT 8054
#define BUFFER_SIZE 256
#define MAX_BOWMANS 20
#define FILE_MAX_DATA_SIZE (BUFFER_SIZE - 3 - strlen("FILE_DATA"))
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
    char *filename;
    int ID;
    long long fileSize;

} ThreadInfo;

typedef struct {
    long ID;
    char data[BUFFER_SIZE - 3 - 9];
} queueMessage;

#endif  // STRUCT_DEFINITIONS_H