#define _GNU_SOURCE
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "io_utils.h"
#include "struct_definitions.h"

// arnau.vives I3_6

void sendSocketMessage(int socketFD, SocketMessage message);
SocketMessage getSocketMessage(int clientFD);
int createAndListenSocket(char *IP, int port);
int createAndConnectSocket(char *IP, int port, int isVerbose);
void sendError(int clientFD);
int sendSocketFile(int socketFD, SocketMessage message, int dataLength);