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

void afficher_temperature(const MessageTemperature *msg);

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

    printf("Console de type E connectée à gestion_console.\n");

    // Envoi du message d'identification
    MessageTemperature identification = {0, MessageTemperature.MESSAGE_TYPE_MESURE, "ConsoleTypeE"};
    serializeMessage(&identification, buffer, &length);

    if (send(tcp_socket, buffer, length, 0) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur d'envoi de l'identification : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Message d'identification envoyé à gestion_console.\n");

    // Boucle principale pour recevoir les messages
    while (1)
    {
        int recv_len = recv(tcp_socket, buffer, BUFFER_SIZE - 1, 0);
        if (recv_len > 0)
        {
            buffer[recv_len] = '\0'; // Terminaison de la chaîne
            deserializeMessage(buffer, recv_len, &msg);

            if (!validateMessage(&msg))
            {
                fprintf(stderr, "Message non valide recu.\n");
                continue;
            }

            // Afficher les informations de température
            afficher_temperature(&msg);
        }
        else if (recv_len == 0)
        {
            printf("Connexion fermée par gestion_console.\n");
            break;
        }
        else
        {
            fprintf(stderr, "Erreur de réception TCP : %d\n", WSAGetLastError());
            break;
        }
    }

    // Fermeture de la socket
    closesocket(tcp_socket);
    WSACleanup();
    return 0;
}

// Fonction pour afficher les informations de température
void afficher_temperature(const MessageTemperature *msg)
{
    printf("******---------------------------******\n");
    printf("\n--- Informations recues ---\n");
    printf("Piece : %s\n", msg->piece);
    printf("Type : %d\n", msg->type);
    printf("Valeur : %d\n", msg->valeur);
    printf("******---------------------------******\n");
}
