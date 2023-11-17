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
  char *firstIP;
  int firstPort;
  char *secondIP;
  int secondPort;
} Poole;

typedef struct
{
  char *firstIP;
  int firstPort;
  char *secondIP;
  int secondPort;
} Discovery;


#endif // STRUCT_DEFINITIONS_H