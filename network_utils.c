#include "network_utils.h"
#include "io_utils.h"
#include "struct_definitions.h"

// arnau.vives joan.medina I3_6

void sendSocketMessage(int socketFD, SocketMessage message)
{
  char *buffer = malloc(sizeof(char) * 256);
  buffer[0] = message.type;
  buffer[1] = (message.headerLength & 0xFF);
  buffer[2] = ((message.headerLength >> 8) & 0xFF);

  for (int i = 0; i < message.headerLength; i++)
  {
    buffer[i + 3] = message.header[i];
  }

  size_t i;
  int start_i = 3 + strlen(message.header);
  for (i = 0; i < strlen(message.data) && message.data != NULL; i++)
  {
    buffer[i + start_i] = message.data[i];
    // printf("buffer[%ld] = %c\n", i + start_i, buffer[i + start_i]);
  }

  int start_j = strlen(message.data) + start_i;
  for (int j = 0; j < 256 - start_j; j++)
  {
    buffer[j + start_j] = '%';
    // printf("buffer[%d] = %c\n", j + start_j, buffer[j + start_j]);
  }

  write(socketFD, buffer, 256);
}

int createAndBindSocket(char *IP, int port)
{
  char *buffer;
  asprintf(&buffer, "Creating socket on %s:%d\n", IP, port);
  printToConsole(buffer);
  free(buffer);

  int socketFD;
  struct sockaddr_in server;

  if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  bzero(&server, sizeof(server));
  server.sin_port = htons(port);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_pton(AF_INET, IP, &server.sin_addr);

  if (inet_pton(AF_INET, IP, &server.sin_addr) <= 0)
  {
    printError("Error converting the IP address\n");
    exit(1);
  }

  printToConsole("Socket created\n");

  if (bind(socketFD, (struct sockaddr *)&server, sizeof(server)) < 0)
  {
    printToConsole("HERE\n");
    printError("Error binding the socket\n");
    exit(1);
  }

  printToConsole("Socket binded\n");

  if(listen(socketFD, 10) < 0)
  {
    printError("Error listening\n");
    exit(1);
  }

  return socketFD;
}

SocketMessage getSocketMessage(int clientFD)
{

  SocketMessage message;
  // get the type
  uint8_t type;
  read(clientFD, &type, 1);
  printf("Type: 0x%02x\n", type);
  message.type = type;

  // get the header length
  uint16_t headerLength;
  read(clientFD, &headerLength, sizeof(unsigned short));
  printf("Header length: %u\n", headerLength);
  message.headerLength = headerLength;

  // get the header
  char *header = malloc(sizeof(char) * headerLength + 1);
  read(clientFD, header, headerLength);
  header[headerLength] = '\0';
  printf("Header: %s\n", header);
  message.header = header;

  // get the data
  char *data = readUntil('%', clientFD);
  char *buffer;
  asprintf(&buffer, "Data: %s\n", data);
  printToConsole(buffer);

  message.data = data;

  return message;
}