#ifndef STRUCT_DEFINITIONS_H
#define STRUCT_DEFINITIONS_H

#include <stdint.h>

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
  Bowman *bowmans;
} PooleServer;

typedef struct {
  int socketFD;
  char *filename;
  int ID;
  long long fileSize;
  
} ThreadInfo;

#define TRUE 1
#define FALSE 0

#define CON_OK "CON_OK"
#define DOWNLOAD_SONG "DOWNLOAD_SONG"
#define DOWNLOAD_SONG_PORT 8054

#define BUFFER_SIZE 256

#endif // STRUCT_DEFINITIONS_H