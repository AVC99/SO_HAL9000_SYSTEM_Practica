#include "io_utils.h"
#include "struct_definitions.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>

void sendSocketMessage(int socketFD, SocketMessage message);
SocketMessage getSocketMessage(int clientFD);