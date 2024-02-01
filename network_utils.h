#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "io_utils.h"
#include "struct_definitions.h"

void sendSocketMessage(int socketFD, SocketMessage message);
SocketMessage getSocketMessage(int clientFD);
int createAndListenSocket(char *IP, int port);
int createAndConnectSocket(char *IP, int port);
void sendError(int clientFD);