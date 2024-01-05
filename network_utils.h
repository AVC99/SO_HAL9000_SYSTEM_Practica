#include "io_utils.h"
#include "struct_definitions.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>

void sendSocketMessage(int socketFD, SocketMessage message);
SocketMessage getSocketMessage(int clientFD);
int createAndListenSocket(char *IP, int port);
int createAndConnectSocket(char *IP, int port);
void sendError(int clientFD);