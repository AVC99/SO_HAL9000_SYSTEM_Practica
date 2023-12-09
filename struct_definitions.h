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

#endif // STRUCT_DEFINITIONS_H