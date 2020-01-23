//------------------------------------
// Projet2 : Client-serveur
// Bruno Cornil matricule : 000392857
//------------------------------------


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include "server.h"

#define BUFFERSIZE 200

// ------------------------------------------------------------------------------
// main: Point d'entrée du programme (exige un paramètre pour indiquer le nom ou
// l'adresse IP du serveur)
// ------------------------------------------------------------------------------
int main (int argc, char* argv[]) {

   int clientSocket;
   int sizeofSockaddr;
   struct sockaddr_in  serverAddr;
   struct hostent *serverHostent;
   fd_set fdSocketRead;
   struct timeval tv;
   int selectResult;
   int isExit;
   int numBytes;

   sizeofSockaddr = sizeof(struct sockaddr_in);

   if (argc != 2) {
     fprintf(stderr, "ERROR, wrong or missing arguments\n Usage:%s <addresse IP ou nom du serveur>\n", argv[0]);
     return EXIT_FAILURE;
   }

   if ((serverHostent=gethostbyname(argv[1])) == NULL) {
     perror("gethostbyname error");
     return EXIT_FAILURE;
   }

   // Création du socket
   clientSocket = socket(AF_INET, SOCK_STREAM, 0);
   if (clientSocket == -1) {
      perror("socket error");
      exit(EXIT_FAILURE);
   }

   // Création de la structure de socket
   memset(&serverAddr, 0, sizeofSockaddr);
   serverAddr.sin_family = AF_INET;
   serverAddr.sin_port = htons(SERVERPORT);
   serverAddr.sin_addr = *((struct in_addr*)serverHostent->h_addr);

   // Connection au serveur
   if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeofSockaddr) == -1) {
      perror("connect error");
      exit(EXIT_FAILURE);
   }

   // Boucle indéfiniment tant qu'il n'y a pas d'erreur de communication
   // (sauf CTRL-C le serveur coupe la communication)
   isExit=0;
   while (isExit == 0) {
      // timer de 0.1s
      tv.tv_sec=0;
      tv.tv_usec=100000;

      // Ajoute stdin et le socket sur la liste des sources à écouter
      FD_ZERO(&fdSocketRead);
      FD_SET(0,&fdSocketRead);
      FD_SET(clientSocket,&fdSocketRead);

      selectResult = select(clientSocket+1, &fdSocketRead, NULL, NULL, &tv);

      // Erreur sur le select ... on quitte le programme
      if (selectResult == -1) {
         perror("select() error");
         exit(EXIT_FAILURE);

      // Pas d'activité (100ms timeout)
      } else if (selectResult == 0) {

      // Il y a de l'activité sur stdin ou le socket
      } else {
         // Réception des données du serveur
         if (FD_ISSET(clientSocket, &fdSocketRead)) {
            char rxBuffer[BUFFERSIZE];

            numBytes = recv(clientSocket, rxBuffer, BUFFERSIZE-1, 0);
            // le serveur n'envoie jamais de données vide
            if (numBytes <= 0) {
               isExit = 1;
            } else {
               // réceptionne et affiche les données recues
               rxBuffer[numBytes]=0;
               printf("%s", rxBuffer);
               fflush(stdout);
            }
         }
         // Lecture de stdin
         if (FD_ISSET(0, &fdSocketRead)) {
            char *txBuffer;
            size_t bufSize = 0;

            numBytes = getline((char **)&txBuffer, &bufSize, stdin);
            // Envoie le buffer au serveur
            if (send(clientSocket, txBuffer, numBytes, 0) == -1) {
              isExit = 0;
              perror("send error");
            }
            free(txBuffer);
         }
      }
   }
   return 0;
}
