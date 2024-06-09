#include "io_utils.h"
#include "network_utils.h"
#include "struct_definitions.h"

// arnau.vives joan.medina I3_6
/**
 * @brief Sends a message through a socket this does not work with sending files
 * @param socketFD The socket file descriptor
 * @param message The message to send
 */
void sendSocketMessage(int socketFD, SocketMessage message) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[0] = message.type;
    buffer[1] = (message.headerLength & 0xFF);
    buffer[2] = ((message.headerLength >> 8) & 0xFF);

    for (int i = 0; i < message.headerLength; i++) {
        buffer[i + 3] = message.header[i];
    }

    size_t i;
    int start_i = 3 + strlen(message.header);
    for (i = 0; i < strlen(message.data) && message.data != NULL; i++) {
        buffer[i + start_i] = message.data[i];
        // printf("buffer[%ld] = %c\n", i + start_i, buffer[i + start_i]);
    }

    int start_j = strlen(message.data) + start_i;
    for (int j = 0; j < 256 - start_j; j++) {
        buffer[j + start_j] = '\0';
        // printf("buffer[%d] = %c\n", j + start_j, buffer[j + start_j]);
    }

    write(socketFD, buffer, 256);
    free(buffer);
}
/**
 * @brief Sends a file (99.9% a chunk of file) through a socket
 * @param socketFD The socket file descriptor
 * @param message The message to send
 * @param dataLength The length of the data
 */
void sendSocketFile(int socketFD, SocketMessage message, int dataLength) {
    char *buffer = malloc(sizeof(char) * 256);
    buffer[0] = message.type;
    buffer[1] = (message.headerLength & 0xFF);
    buffer[2] = ((message.headerLength >> 8) & 0xFF);
    size_t dataLength_ = (size_t)dataLength;

    for (size_t i = 0; i < message.headerLength; i++) {
        buffer[i + 3] = message.header[i];
    }

    size_t start_i = 3 + message.headerLength;
    for (size_t i = 0; i < dataLength_ && message.data != NULL; i++) {
        buffer[i + start_i] = message.data[i];
    }

    size_t start_j = dataLength_ + start_i;
    int paddingCounter = 0;
    for (size_t j = 0; j < 256 - start_j; j++) {
        buffer[j + start_j] = '\0';
        //printToConsole("I ADDED PADDING\n");
        paddingCounter++;
    }
    printf("Padding added: %d\n", paddingCounter);

    ssize_t bytesWritten = write(socketFD, buffer, 256);
    if (bytesWritten < 0) {
        printError("Error writing to the socket\n");
    }
    free(buffer);
}

/**
 * @brief Creates a socket and connects it to a server
 * @param IP The IP address of the server
 * @param port The port of the server
 * @return The socket file descriptor
 */
int createAndConnectSocket(char *IP, int port) {
    char *buffer;
    asprintf(&buffer, "Creating and connecting socket on %s:%d\n", IP, port);
    printToConsole(buffer);
    free(buffer);

    int socketFD;
    struct sockaddr_in server;

    if ((socketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        printError("Error creating the socket\n");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_port = htons(port);
    server.sin_family = AF_INET;

    // Check if the IP is valid and if it failed to convert, check why
    if (inet_pton(AF_INET, IP, &server.sin_addr) <= 0) {
        asprintf(&buffer, "IP address: %s\n", IP);
        printToConsole(buffer);
        free(buffer);

        if (inet_pton(AF_INET, IP, &server.sin_addr) == 0) {
            printError("inet_pton() failed: invalid address string\n");
        } else {
            printError("inet_pton() failed\n");
        }
        printError("Error converting the IP address\n");
        exit(1);
    }

    // Connect to server
    if (connect(socketFD, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printError("Error connecting\n");
        exit(1);
    }

    return socketFD;
}
/**
 * @brief Creates a socket and listens to it
 * @param IP The IP address of the server
 * @param port The port of the server
 */
int createAndListenSocket(char *IP, int port) {
    char *buffer;
    asprintf(&buffer, "Creating socket on %s:%d\n", IP, port);
    printToConsole(buffer);
    free(buffer);

    int socketFD;
    struct sockaddr_in server;

    if ((socketFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printError("Error creating the socket\n");
        exit(1);
    }

    bzero(&server, sizeof(server));
    server.sin_port = htons(port);
    server.sin_family = AF_INET;

    // Check if the IP is valid and if it failed to convert, check why
    if (inet_pton(AF_INET, IP, &server.sin_addr) <= 0) {
        char *buffer;
        asprintf(&buffer, "IP address: %s\n", IP);
        printToConsole(buffer);
        free(buffer);

        if (inet_pton(AF_INET, IP, &server.sin_addr) == 0) {
            printError("inet_pton() failed: invalid address string\n");
        } else {
            printError("inet_pton() failed\n");
        }
        printError("Error converting the IP address\n");
        exit(1);
    }

    printToConsole("Socket created\n");

    if (bind(socketFD, (struct sockaddr *)&server, sizeof(server)) < 0) {
        printError("Error binding the socket\n");
        exit(1);
    }

    printToConsole("Socket binded\n");

    if (listen(socketFD, 10) < 0) {
        printError("Error listening\n");
        exit(1);
    }

    return socketFD;
}

SocketMessage getSocketMessage(int clientFD) {
    char *buffer;
    SocketMessage message;
    ssize_t numBytes;

    // get the type
    uint8_t type;
    numBytes = read(clientFD, &type, 1);
    if (numBytes < 1) {
        printError("Error reading the type\n");
    }
    asprintf(&buffer, "Type: 0x%02x\n", type);
    printToConsole(buffer);
    free(buffer);
    message.type = type;

    // get the header length
    uint16_t headerLength;
    numBytes = read(clientFD, &headerLength, sizeof(unsigned short));
    if (numBytes < (ssize_t)sizeof(unsigned short)) {
        printError("Error reading the header length\n");
    }
    asprintf(&buffer, "Header length: %u\n", headerLength);
    printToConsole(buffer);
    free(buffer);
    message.headerLength = headerLength;

    // get the header
    char *header = malloc(sizeof(char) * headerLength + 1);
    numBytes = read(clientFD, header, headerLength);
    if (numBytes < headerLength) {
        printError("Error reading the header\n");
        free(header);
    }
    header[headerLength] = '\0';
    asprintf(&buffer, "Header: %s\n", header);
    printToConsole(buffer);
    free(buffer);
    message.header = strdup(header);
    free(header);

    // get the data
    char *data = malloc(sizeof(char) * 256 - 3 - headerLength + 1);
    numBytes = read(clientFD, data, 256 - 3 - headerLength);
    if (numBytes < 256 - 3 - headerLength) {
        printError("Error reading the data\n");
        free(data);
    }
    asprintf(&buffer, "Data: %s\n", data);
    printToConsole(buffer);
    free(buffer);

    message.data = strdup(data);
    free(data);

    return message;
}
/**
 * @brief Sends the default error message through a socket
 * @param clientFD The socket file descriptor
 */
void sendError(int clientFD) {
    SocketMessage message;
    message.type = 0x07;
    message.headerLength = strlen("UNKNOWN");
    message.header = "UNKNOWN";
    message.data = "";
    sendSocketMessage(clientFD, message);
}