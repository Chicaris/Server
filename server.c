//------------------------------------
// Projet2 : Client-serveur
// Bruno Cornil matricule : 000392857
//------------------------------------

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <error.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include "server.h"

#define BUFFERSIZE 200

#define STATE_CONNECTED 0
#define STATE_NICKNAME  1
#define STATE_PROMPT    2
#define STATE_MESSAGE   3
#define STATE_END       4

struct clientNode {
   int state;
   int socket;
   char nickName[10];
   struct clientNode *destination;
   struct clientNode *next;
};

// Noeud racine de la liste des clients
struct clientNode *clientNodeRoot;

// ------------------------------------------------------------------------------
// cleanNodeList : Retire de la liste tous les clients dans l'état 'STATE_END'
// ------------------------------------------------------------------------------
void cleanNodeList() {
   struct clientNode *currentNodePtr, *parentNodePtr;

   parentNodePtr = NULL;
   currentNodePtr = clientNodeRoot;
   // Parcours la liste de clients
   while (currentNodePtr != NULL) {
      // La thread client est terminée
      if (currentNodePtr->state == STATE_END) {
         printf("Free resources for socket  %d\n", currentNodePtr->socket);
         close(currentNodePtr->socket);
         // Supression du premier élément de la liste
         if (parentNodePtr == NULL)  {
            clientNodeRoot = currentNodePtr->next;
            close(currentNodePtr->socket); 
            free(currentNodePtr); 
            currentNodePtr = clientNodeRoot;
         // Suppression d'un élément dans la liste
         } else {
            parentNodePtr->next = currentNodePtr->next;
            free(currentNodePtr); 
            currentNodePtr = parentNodePtr->next;
         }
      // Snon, avance sur le noeud suivant
      } else {
         parentNodePtr = currentNodePtr;
         currentNodePtr = currentNodePtr->next;
      }
   }
}

// ------------------------------------------------------------------------------
// usage: retourne une chaine de caractères contenant la liste des commandes
// ------------------------------------------------------------------------------
char *usage() {
   return
"Available commands\n\
h- This help\n\
l- List connected users\n\
m <user> - Send a message to <user>\n\
q- Quit\n\
\n\
>> ";
}

// ------------------------------------------------------------------------------
// getClient: Retourne le pointeur du client dans la liste correspondant à un nom
// ------------------------------------------------------------------------------
struct clientNode *getClient(char* nickname) {
   struct clientNode *currentNodePtr;

   currentNodePtr = clientNodeRoot;
   // Parcours la liste de clients
   while (currentNodePtr != NULL) {
      if(strcmp(nickname, currentNodePtr->nickName) == 0) {
         break;
      }
      currentNodePtr = currentNodePtr->next;
   }
   return currentNodePtr;
}

// ------------------------------------------------------------------------------
// clientSend: Envoie le contenu du buffer au client marque celui-ci comme
// terminé l'envoi à échoué (libère le client de la liste et les ressources
// allouées
// ------------------------------------------------------------------------------
void clientSend(struct clientNode *this, char* buffer) {
   if (send(this->socket, buffer, strlen(buffer), 0) == -1) {
      perror("Client disconnected");
      this->state = STATE_END;
   }
}

// ------------------------------------------------------------------------------
// clientReceive: Reçoit les données d'un client, les places dans un buffer
// et retourne le nombre de bytes reçus. Si on ne reçoit rien, c'est anormal
// et on libère le client
// ------------------------------------------------------------------------------
int clientReceive(struct clientNode *this, char* buffer, int maxSize) {
   int numBytes;

   numBytes = recv(this->socket, buffer, maxSize, 0);
   if (numBytes == 0) {
      this->state = STATE_END;
   }
   return numBytes;
}
// ------------------------------------------------------------------------------
// printState: Affiche l'état d'un client
// ------------------------------------------------------------------------------
void printState(struct clientNode *this, char* endString) {
   printf("Client [%s] (", this->nickName);
   switch (this->state) {
      case STATE_CONNECTED:
         printf("CONNECTED)");
         break;
       case STATE_NICKNAME:
         printf("NICKNAME)");
         break;
       case STATE_PROMPT:
         printf("PROMPT)");
         break;
       case STATE_MESSAGE:
         printf("MESSAGE)");
         break;
       case STATE_END:
         printf("END)");
         break;
   }
   printf("%s", endString);
}

// ------------------------------------------------------------------------------
// clientProcess: Machine d'état appelée si qqch a été reçu sur le socket pour
// le client donné par le pointeur 'this'.
// ------------------------------------------------------------------------------
void clientProcess(struct clientNode *this) {
   char buffer[BUFFERSIZE];
   int numBytes;
   int i, idx;
   struct clientNode *currentNodePtr;

#ifdef DEBUG
   printState(this, " -> ");
#endif
   switch (this->state) {
      // La connection est initialisée (rien à lire, appelé lors de la création du client),
      // on envoie le prompt et on passe à l'état suivant
      case STATE_CONNECTED:
         this->state = STATE_NICKNAME;
         clientSend(this, "*** INFO-F201 Message server ***\nEnter your nickname (max. 10 char.): ");
         break;

      // Le nom fournit par l'utilisateur est comparé aux clients existant, on enregistre
      // celui-ci et passe dans l'état suivant si celui-ci n'existe pas encore sinon on
      // envoie un message d'erreur
      case STATE_NICKNAME:
         if ((numBytes = clientReceive(this, buffer, 10)) == 0) {
            this->state = STATE_END;
         } else {
            // On supprime le '\n' reçu
            buffer[numBytes-1]=0;
            if (getClient(buffer) != NULL) {
               sprintf(buffer, "Nickname used already, enter a different one:");
            } else {
               strcpy(this->nickName, buffer);
               sprintf(buffer, "\nWelcome %s !\n\n", this->nickName);
               strcat(buffer, (const char *) usage());
               this->state = STATE_PROMPT;
            }
            clientSend(this, buffer);
         }
         break;

      // On attend une commande de l'utilisateur
      case STATE_PROMPT:
         if ((numBytes=clientReceive(this, buffer, BUFFERSIZE)) == 0) {
            this->state = STATE_END;
         } else {
            switch (buffer[0]) {
               // Retourne la liste des utilisateurs existants (et on s'exclu de la liste)
               case 'l': 
                  clientSend(this, "List of users: ");
                  currentNodePtr = clientNodeRoot;
                  buffer[0]=0;
                  while (currentNodePtr != NULL) {
                     if (currentNodePtr != this && strcmp(currentNodePtr->nickName, "") !=0) {
                        if (buffer[0] != 0) {
                           strcpy(buffer, ", ");
                        }
                        strcat(buffer, currentNodePtr->nickName);
                        clientSend(this, buffer);
                     }
                     currentNodePtr = currentNodePtr->next;
                  }
                  if (buffer[0] != 0) {
                     clientSend(this, "\n>> ");
                  } else {
                     clientSend(this, "<none>\n>> ");
                  }
                  break;

               // L'utilisateur veut envoyer un message, on vérifie que le destinataire existe
               // et si cest le cas, on enregistre celui-ci dans la strcuture du client et on 
               // passe dans l'état STATE_MESSAGE
               case 'm':
                  idx=0;
                  for (i=1;i<numBytes;i++) {
                     if (buffer[i] != ' ') {
                        buffer[idx++]=buffer[i];
                     }
                  }
                  // On supprime le '\n' reçu
                  buffer[idx-1] = 0;
                  if ((currentNodePtr=getClient(buffer)) != NULL) {
                     sprintf(buffer, "Message to: %s\n", currentNodePtr->nickName);
                     this->destination=currentNodePtr;
                     this->state = STATE_MESSAGE;
                  } else {
                     strcpy(buffer, "Unknown user\n>> ");
                  }  
                  clientSend(this, buffer);
                  break;

               case 'h':
                  clientSend(this, usage());
                  break;

               case 'q': 
                  clientSend(this, "Bye !\n");
                  this->state = STATE_END;
                  break;

               default:
                  clientSend(this, "Invalid command !\n\n");
                  clientSend(this, usage());
                  break;
            }
         }
         break;

      // Les données recues sont envoyées au destinataire
      case STATE_MESSAGE:
         if ((numBytes = clientReceive(this, buffer, BUFFERSIZE)) == 0) {
            this->state = STATE_END;
         } else {
            buffer[numBytes]=0;
            // On pourrait créer un deuxième buffer et tout envoyer en une fois
            // et n'afficher 'Message from...' qu'une fois pour des longs messages segmentés
            clientSend(this->destination, "\nMessage from:");
            clientSend(this->destination, this->nickName);
            clientSend(this->destination, "\n");
            clientSend(this->destination, buffer);
            // On retourne à l'état STATE_PROMPT si on a reçu tout le message ('\n' est
            // le dernier charactère du message)
            if (buffer[numBytes-1] == '\n') {
               clientSend(this->destination, ">> ");
               clientSend(this, ">> ");
               this->state = STATE_PROMPT;
            }
         }
         break;

      case STATE_END:
         break;
   }
#ifdef DEBUG
   printState(this, "\n");
#endif
}

// ------------------------------------------------------------------------------
// main: Point d'entrée du programme (pas de paramètres)
// ------------------------------------------------------------------------------
int main (int argc, char* argv[]) {
   int serverSocket;
   struct sockaddr_in serverAddr;
   int clientSocket;
   struct sockaddr_in clientAddr;
   int sizeofSockaddr;
   struct clientNode *clientNodePtr;
   fd_set fdSocketRead;
   struct timeval tv;
   int selectResult;
   int maxDesc;

   clientNodeRoot=NULL;
   sizeofSockaddr = sizeof(struct sockaddr_in);

   // Creation du socket
   serverSocket = socket(AF_INET, SOCK_STREAM, 0);
   if (serverSocket == -1) {
      perror("socket error");
      exit(EXIT_FAILURE);
   }
   printf("Socket created\n");

   // Creation de la structure de socket
   memset(&serverAddr, 0, sizeofSockaddr);
   serverAddr.sin_family = AF_INET;
   serverAddr.sin_port = htons(SERVERPORT);
   serverAddr.sin_addr.s_addr = INADDR_ANY;

   // Liaison du socket à la structure ci-dessus
   int yes = 1;
   setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
   if (bind(serverSocket, (struct sockaddr *) & serverAddr, sizeofSockaddr) != 0) {
      perror("bind (socket) error");
      exit(EXIT_FAILURE);
   }
   printf("Socket bound to port %d\n", SERVERPORT);

   // Demarre l'ecoute sur le socket (max. 20 connections en attente)
   if (listen(serverSocket, 20) !=0) {
      perror("listen (socket) error");
      exit(EXIT_FAILURE);
   }
   printf("Listening for clients\n");

   while(1) {
      // Attends une activité sur un des sockets pendant 100ms
      tv.tv_sec=0;
      tv.tv_usec=100000;
      FD_ZERO(&fdSocketRead);
      FD_SET(serverSocket,&fdSocketRead);
      maxDesc = serverSocket;
      clientNodePtr = clientNodeRoot;
      while (clientNodePtr != NULL) {
         if (clientNodePtr->state != STATE_END) {
            FD_SET(clientNodePtr->socket,&fdSocketRead);
            maxDesc = (clientNodePtr->socket > maxDesc ? clientNodePtr->socket : maxDesc);
         }
         clientNodePtr = clientNodePtr->next;
      }
      selectResult = select(maxDesc+1, &fdSocketRead, NULL, NULL, &tv);

      if (selectResult == -1) {
         perror("select() error");
         exit(EXIT_FAILURE);

      // Pas d'activité (100ms timeout)
      } else if (selectResult == 0) {
         // on nettoie la liste de noeuds
         cleanNodeList(&clientNodeRoot); 

      // Il y a de l'activité sur un socket
      } else {
         if (FD_ISSET(serverSocket, &fdSocketRead)) {
            // Socket du serveur
            clientSocket=accept(serverSocket,(struct sockaddr*)&clientAddr,(socklen_t *) &sizeofSockaddr);
            printf("New connection with %s (port %d)\n", inet_ntoa(clientAddr.sin_addr), (int) ntohs(clientAddr.sin_port));

            // Alloue et initialise un noeud pour un nouveau client
            clientNodePtr = (struct clientNode *) malloc(sizeof(struct clientNode));
            if (clientNodePtr == NULL) {
               perror("malloc error");
               exit(EXIT_FAILURE);
            }
      
            // Insère le nouveau noeud au début de la liste
            memset(clientNodePtr, 0, sizeof(*clientNodePtr));
            clientNodePtr->socket = clientSocket;
            clientNodePtr->destination = NULL;
            strcpy(clientNodePtr->nickName, "");
            clientNodePtr->next = clientNodeRoot;
            clientNodePtr->state = STATE_CONNECTED;
            clientNodeRoot = clientNodePtr;
            clientProcess(clientNodePtr);

         } else {
            // Socket d'un/de client(s)
            clientNodePtr = clientNodeRoot;
            while (clientNodePtr != NULL) {
               if (clientNodePtr->state != STATE_END && FD_ISSET(clientNodePtr->socket, &fdSocketRead)) {
                  clientProcess(clientNodePtr);
               }
               clientNodePtr = clientNodePtr->next;
            }
         }
      }
   }
   //On arrive jamais ici ...
   return 0;
}
