#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <stdint.h>
#include "message_temperature.h"

#pragma comment(lib, "ws2_32.lib")

#define GESTION_CONSOLE_PORT 54323
#define BUFFER_SIZE 1024

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

int main()
{
    WSADATA wsaData;
    SOCKET tcp_socket;
    SOCKADDR_IN gestion_console_addr;
    char buffer[BUFFER_SIZE];
    MessageTemperature msg;
    size_t length;

    // Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Erreur d'initialisation de Winsock : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Création de la socket TCP
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur création socket TCP : %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse de gestion_console
    memset(&gestion_console_addr, 0, sizeof(gestion_console_addr));
    gestion_console_addr.sin_family = AF_INET;
    gestion_console_addr.sin_port = htons(GESTION_CONSOLE_PORT);
    gestion_console_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // Connexion à gestion_console
    if (connect(tcp_socket, (SOCKADDR *)&gestion_console_addr, sizeof(gestion_console_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur connexion à gestion_console : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Console de type C connectee a gestion_console.\n");

    // Envoi du message d'identification
    MessageTemperature identification;
    strcpy(identification.piece, "ConsoleTYPE=C"); //Je copie la chaine
    identification.type = MESSAGE_TYPE_MESURE;
    identification.valeur = 0;
    serializeMessage(&identification, buffer, &length);

    if (send(tcp_socket, buffer, length, 0) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur d'envoi de l'identification : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Message d'identification envoye a gestion_console.\n");

    // Boucle pour envoyer des commandes de chauffage
    while (1)
    {
        char piece[50];
        int puissance;

        printf("Entrez le nom de la piece : ");
        scanf("%s", piece);
        printf("Entrez la puissance de chauffage (0-5) : ");
        scanf("%d", &puissance);

        // Construction du message de chauffage
        msg.type = MESSAGE_TYPE_CHAUFFER;
        msg.valeur = puissance;
        strncpy(msg.piece, piece, sizeof(msg.piece));

        // Sérialisation et envoi
        serializeMessage(&msg, buffer, &length);
        if (send(tcp_socket, buffer, length, 0) == SOCKET_ERROR)
        {
            fprintf(stderr, "Erreur d'envoi TCP : %d\n", WSAGetLastError());
        }
        else
        {
            printf("Commande envoyee : piece=%s, puissance=%d\n", msg.piece, msg.valeur);
        }
    }

    // Fermeture de la socket
    closesocket(tcp_socket);
    WSACleanup();
    return 0;
}
