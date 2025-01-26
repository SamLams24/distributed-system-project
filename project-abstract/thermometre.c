#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <stdint.h>
#include <stdbool.h>
#include <ws2tcpip.h>
#include "message_temperature.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

/*-liphlpapi -lws2_32*/

#define BUFFER_SIZE 1024

#define MULTICAST_GROUP "224.0.0.1"
#define MULTICAST_PORT 12345
#define CENTRAL_SYSTEM_IP "127.0.0.1"
#define CENTRAL_SYSTEM_PORT 54321

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

void identifier_thermometre(SOCKET udp_socket, SOCKET tcp_socket, SOCKADDR_IN multicast_addr);

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    int udp_socket, tcp_socket;
    SOCKADDR_IN multicast_addr, central_addr;
    uint8_t buffer[256];
    MessageTemperature msg;

    // Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Erreur initialisation Winsock : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Création du socket UDP
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur creation socket UDP : %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse multicast
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(MULTICAST_PORT);
    multicast_addr.sin_addr.s_addr = INADDR_ANY;

    // Liaison du socket au groupe multicast
    if (bind(udp_socket, (SOCKADDR *)&multicast_addr, sizeof(multicast_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur de liaison UDP : %d\n", WSAGetLastError());
        closesocket(udp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Rejoindre le groupe multicast
    struct ip_mreq multicast_request;
    multicast_request.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    multicast_request.imr_interface.s_addr = INADDR_ANY;
    if (setsockopt(udp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multicast_request, sizeof(multicast_request)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur de jointure au groupe multicast : %d\n", WSAGetLastError());
        closesocket(udp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    printf("Groupe Multicast rejoint avec Succes !!!!\n");

    // Création du socket TCP pour communication avec le système central
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur creation socket TCP : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    memset(&central_addr, 0, sizeof(central_addr));
    central_addr.sin_family = AF_INET;
    central_addr.sin_port = htons(CENTRAL_SYSTEM_PORT);
    central_addr.sin_addr.s_addr = inet_addr(CENTRAL_SYSTEM_IP);

    if (connect(tcp_socket, (SOCKADDR *)&central_addr, sizeof(central_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur connexion au systeme central : %d\n", WSAGetLastError());
        closesocket(udp_socket);
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Connexion au systeme Central reussit !!\n");

    identifier_thermometre(udp_socket, tcp_socket, multicast_addr);

    // Boucle de réception des données multicast
    while (true)
    {
        socklen_t addrlen = sizeof(multicast_addr);
        int recvlen = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&multicast_addr, &addrlen);

        if (recvlen > 0)
        {
            buffer[recvlen] = '\0';
            deserializeMessage(buffer, recvlen, &msg);

            if (!validateMessage(&msg))
            {
                fprintf(stderr, "Message non valide : type=%d, valeur=%d, pièce=%s\n", msg.type, msg.valeur, msg.piece);
                continue;
            }

            printf("Message recu : type=%d, temperature=%d, piece=%s\n",
                   msg.type, msg.valeur, msg.piece);

            // Apres reception on verifie si le message est de type MESURE pour l'envoyer au systeme Central

            if (msg.type != MESSAGE_TYPE_MESURE)
            {
                fprintf(stderr, "Message non pertinent pour le thermomètre, ignoré.\n");
                continue;
            }

            // Envoi au système central
            if (send(tcp_socket, buffer, recvlen, 0) == SOCKET_ERROR)
            {
                fprintf(stderr, "Erreur d'envoi TCP : %d\n", WSAGetLastError());
                closesocket(udp_socket);
                closesocket(tcp_socket);
                WSACleanup();
                exit(EXIT_FAILURE);
            }

            printf("Message envoye au systeme central avec succes !\n"); // Ajout de l'affichage des données send si problème coté serveur
        }
    }

    // Fermeture des sockets et nettoyage
    closesocket(udp_socket);
    closesocket(tcp_socket);
    WSACleanup();
    return 0;
}

void identifier_thermometre(SOCKET udp_socket, SOCKET tcp_socket, SOCKADDR_IN multicast_addr)
{
    uint8_t buffer[BUFFER_SIZE];
    MessageTemperature msg;
    socklen_t addrlen = sizeof(multicast_addr);

    printf("Thermometre en attente de messages multicast pour identification...\n");

    while (true)
    {
        int recvlen = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&multicast_addr, &addrlen);
        if (recvlen > 0)
        {
            deserializeMessage(buffer, recvlen, &msg);

            if (msg.type == MESSAGE_TYPE_MESURE)
            {
                printf("Multicast reçu pour thermometre : piece=%s, temperature=%d\n", msg.piece, msg.valeur);

                // Send identification to the central system
                snprintf((char *)buffer, sizeof(buffer), "TYPE=THERMOMETRE;PIECE=%s", msg.piece);
                if (send(tcp_socket, buffer, strlen((char *)buffer), 0) == SOCKET_ERROR)
                {
                    fprintf(stderr, "Erreur lors de l'envoi de l'identification : %d\n", WSAGetLastError());
                    closesocket(tcp_socket);
                    return;
                }
                else
                {
                    printf("Identification envoyee au systeme central : %s\n", buffer);
                }
                break; // Exit the loop after identification
            }
        }
        else if (recvlen == SOCKET_ERROR)
        {
            fprintf(stderr, "Erreur réception multicast : %d\n", WSAGetLastError());
            break;
        }
    }
}
