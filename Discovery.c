#define _GNU_SOURCE
#include "io_utils.h"
#include "struct_definitions.h"
#include "network_utils.h"

#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <pthread.h>
#include <arpa/inet.h>

// arnau.vives joan.medina I3_6

Discovery discovery;
int listenPooleFD, listenBowmanFD, numPooleServers = 0;
PooleServer *pooleServers;

/**
 * Saves the discovery information from the file
 */
void getDiscoveryFromFile(int fd)
{
  printToConsole("Reading configuration file...\n");
  discovery.pooleIP = readUntil('\n', fd);
  char *firstPort = readUntil('\n', fd);
  discovery.poolePort = atoi(firstPort);
  free(firstPort);
  discovery.bowmanIP = readUntil('\n', fd);
  char *secondPort = readUntil('\n', fd);
  discovery.bowmanPort = atoi(secondPort);
  free(secondPort);

  close(fd);
}

/**
 * Frees all the memory allocated from the global Discovery
 */
void freeMemory()
{
  free(discovery.pooleIP);
  free(discovery.bowmanIP);

  close(listenPooleFD);
  close(listenBowmanFD);
}

/**
 * Closes the program
 */
void closeProgram()
{
  freeMemory();
  exit(0);
}

/**
 * Opens a socket and binds it to the given ip and port
 * Listens to posible bowman connections and redirects them to the less crowded poole
 */
void *listenToBowman()
{

  printToConsole("Listening to Bowman...\n");

  if ((listenBowmanFD = createAndBindSocket(discovery.bowmanIP, discovery.bowmanPort)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  while (1)
  {
    printToConsole("\nWaiting for Bowman connections...\n");

    int clientFD = accept(listenBowmanFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }

    printToConsole("\nNew client connected !\n");

    // GET SOCKET DATA
    SocketMessage message = getSocketMessage(clientFD);

    if (strcmp(message.header, "NEW_BOWMAN") == 0)
    {
      printToConsole("NEW_BOWMAN DETECTED\n");

      if (numPooleServers == 0)
      {
        printError("ERROR: No poole servers available\n");
        SocketMessage response;
        response.type = 0x01;
        response.headerLength = strlen("CON_KO");
        response.header = "CON_KO";
        response.data = "";

        sendSocketMessage(clientFD, response);
      }
      else
      {
        char *buffer;
        printToConsole("Sending bowman to the less crowded poole server\n");
        int minBowmans = -1, lowestIndexPoole = -1;
        asprintf(&buffer, "Number of poole servers: %d\n", numPooleServers);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Poole server name: %s\n", pooleServers[0].pooleServername);
        printToConsole(buffer);
        free(buffer);


        for (int i = 0; i <= numPooleServers; i++)
        {

          asprintf(&buffer, "Poole server name: %s\n", pooleServers[i].pooleServername);
          printToConsole(buffer);
          free(buffer);
          if (pooleServers[i].numOfBowmans < minBowmans)
          {
            minBowmans = pooleServers[i].numOfBowmans;
            lowestIndexPoole = i;
          }
        }

        asprintf(&buffer, "Lowest index poole: %d\n", lowestIndexPoole);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Poole server name: %s\n", pooleServers[lowestIndexPoole].pooleServername);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Poole server port: %d\n", pooleServers[lowestIndexPoole].poolePort);
        printToConsole(buffer);
        free(buffer);
        asprintf(&buffer, "Poole server ip: %s\n", pooleServers[lowestIndexPoole].pooleIP);
        printToConsole(buffer);
        free(buffer);

        printToConsole("Poole server found\n");
        SocketMessage response;
        response.type = 0x01;
        response.headerLength = strlen("CON_OK");
        response.header = "CON_OK";

        char *bufferr;
        asprintf(&bufferr, "%s&%d&%s", pooleServers[lowestIndexPoole].pooleServername, pooleServers[lowestIndexPoole].poolePort, pooleServers[lowestIndexPoole].pooleIP);
        response.data = bufferr;

        printToConsole("PETAA\n");
        sendSocketMessage(clientFD, response);

        //???
        // free(buffer);
      }

      // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY i got munmap_chunk(): invalid pointer
      // free(response.header);
      // free(response.data);
    }
    else if (strcmp(message.header, "NEW_POOLE") == 0)
    {
      printToConsole("NEW_POOLE DETECTED\n");
      SocketMessage response;
      response.type = 0x01;
      response.headerLength = strlen("CON_OK");
      response.header = "CON_OK";
      response.data = "";

      sendSocketMessage(clientFD, response);

      // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY i got munmap_chunk(): invalid pointer
      // free(response.header);
      // free(response.data);
    }

    // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY
    //  FREE MEMORY FROM MESSAGE
    free(message.header);
    free(message.data);

    close(clientFD);
  }
  return NULL;
}

/**
 * Opens a socket and binds it to the given ip and port
 * Listens to posible poole connections and adds them to the list of active poole servers
 */
void *listenToPoole()
{

  printToConsole("Listening to Poole...\n");

  if ((listenPooleFD = createAndBindSocket(discovery.pooleIP, discovery.poolePort)) < 0)
  {
    printError("Error creating the socket\n");
    exit(1);
  }

  while (1)
  {
    printToConsole("\nWaiting for Poole connections...\n");

    int clientFD = accept(listenPooleFD, (struct sockaddr *)NULL, NULL);

    if (clientFD < 0)
    {
      printError("Error while accepting\n");
      exit(1);
    }
    printToConsole("\nNew client connected !\n");

    // GET SOCKET DATA
    SocketMessage message = getSocketMessage(clientFD);


    switch (message.type)
    {
    case 0x01:

      if (strcmp(message.header, "NEW_POOLE") == 0)
      {
        printToConsole("NEW_POOLE DETECTED\n");
        SocketMessage response;
        response.type = 0x01;
        response.headerLength = strlen("CON_OK");
        response.header = "CON_OK";
        response.data = "";

        sendSocketMessage(clientFD, response);
        // add the poole to the list of active poole servers
        numPooleServers++;
        printf("numPooleServers: %d\n", numPooleServers);
        // handle possible realloc errors
        PooleServer *temp = realloc(pooleServers, sizeof(PooleServer) * numPooleServers);
        if (temp == NULL)
        {
          printError("Error reallocating memory\n");
          exit(1);
        }
        else
        {
          printf("numPooleServers: %d\n", numPooleServers - 1);
          pooleServers = temp;
          pooleServers->numOfBowmans = 0;

          char *token = strtok(message.data, "&");
          pooleServers[numPooleServers - 1].pooleServername = token;

          token = strtok(NULL, "&");
          pooleServers[numPooleServers - 1].poolePort = atoi(token);

          token = strtok(NULL, "&");
          pooleServers[numPooleServers - 1].pooleIP = token;
          
          //???
          //free(token);
          printToConsole("Poole server added to the list\n");

          char *buffer;
          asprintf(&buffer, "Poole server name: %s\n", pooleServers[numPooleServers - 1].pooleServername);
          printToConsole(buffer);
          free(buffer);
          asprintf(&buffer, "Poole server port: %d\n", pooleServers[numPooleServers - 1].poolePort);
          printToConsole(buffer);
          free(buffer);
          asprintf(&buffer, "Poole server ip: %s\n", pooleServers[numPooleServers - 1].pooleIP);
          printToConsole(buffer);
          free(buffer);
          
        }
      }
      else
      {
        printError("ERROR: Can't establish connection\n");
        SocketMessage response;
        response.type = 0x01;
        response.headerLength = strlen("CON_KO");
        response.header = "CON_KO";
        response.data = "";

        sendSocketMessage(clientFD, response);
      }

      break;

    default:

      printError("Error: Wrong message type\n");
      SocketMessage response;
      response.type = 0x07;
      response.headerLength = strlen("UNKNOWN");
      response.header = "UNKNOWN";
      response.data = "";

      sendSocketMessage(clientFD, response);

      break;
    }

    // TODO: CHECK WHEN I NEED TO FREE THIS MEMORY
    //  FREE MEMORY FROM MESSAGE
    free(message.header);
    free(message.data);

    close(clientFD);
  }

  return NULL;
}

int main(int argc, char *argv[])
{
  pthread_t pooleThread, bowmanThread;
  // Reprogram the SIGINT signal
  signal(SIGINT, closeProgram);

  // Check if the arguments are provided
  if (argc != 2)
  {
    printError("Error: Missing arguments\n");
    return 1;
  }
  int fd = open(argv[1], O_RDONLY);
  if (fd < 0)
  {
    printError("Error: File not found\n");
    return 1;
  }

  getDiscoveryFromFile(fd);

  // Create the threads
  pthread_create(&pooleThread, NULL, (void *)listenToPoole, NULL);
  pthread_create(&bowmanThread, NULL, (void *)listenToBowman, NULL);

  pthread_join(pooleThread, NULL);
  pthread_join(bowmanThread, NULL);
  // listenToPoole();

  // listenToBowman();

  freeMemory();

  return 0;
}