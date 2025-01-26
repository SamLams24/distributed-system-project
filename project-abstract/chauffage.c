#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdbool.h>
#include "message_temperature.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#define MULTICAST_GROUP "224.0.0.1"
#define MULTICAST_PORT 12345
#define CENTRAL_SYSTEM_IP "127.0.0.1"
#define WIFI_INTERFACE_IP "192.168.1.82" // Adresse IP de l'interface Wi-Fi
#define LOCALHOST "127.0.0.1"
#define CENTRAL_SYSTEM_PORT 54321

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#define MAX_PIECES 10
#define BUFFER_SIZE 1024

typedef struct
{
    char nom[256];           // Nom de la Piece
    int chauffage_puissance; // Puissance actuelle du chauffage
    float temperature;       // Température actuelle
} Piece;

Piece pieces[MAX_PIECES];
int nb_pieces = 0;

// Prototypes
void recevoir_commandes_chauffage(SOCKET tcp_socket, SOCKET sockfd, SOCKADDR_IN multicast_addr);
Piece *trouver_ou_ajouter_piece(const char *nom);
void ajuster_temperature(Piece *piece, SOCKET udp_socket, SOCKADDR_IN multicast_addr);
void identifier_chauffage(SOCKET udp_socket, SOCKET tcp_socket, SOCKADDR_IN multicast_addr);

int configure_multicast_interface(int sockfd, const char *interface_ip)
{
    struct in_addr localInterface;
    localInterface.s_addr = inet_addr(interface_ip);

    // Configuration de l'interface réseau pour multicast
    if (setsockopt(sockfd, IPPROTO_IP, IP_MULTICAST_IF, (char *)&localInterface, sizeof(localInterface)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur lors de la configuration de l'interface réseau (%s) : %d\n", interface_ip, WSAGetLastError());
        return SOCKET_ERROR;
    }

    printf("Interface reseau configuree : %s\n", interface_ip);
    return 0;
}

int main(int argc, char *argv[])
{
    WSADATA wsaData;
    SOCKET sockfd, tcp_socket;
    SOCKADDR_IN multicast_addr, central_addr;
    MessageTemperature msg;
    uint8_t buffer[256];
    size_t length;
    struct in_addr localInterface;
    bool use_fallback = false;
    struct ip_mreq multicast_request;

    // Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Erreur d'initialisation Winsock : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Création de la socket UDP
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur de création de la socket : %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Tentons la Configuration de l'interface Wifi , sinon on emettra sur le loopback --> 127.0.0.1
    if (configure_multicast_interface(sockfd, WIFI_INTERFACE_IP) == SOCKET_ERROR)
    {
        fprintf(stderr, "Interface Wi-Fi indisponible, bascule sur le Localhost.\n");

        // Utilisation du localhost
        if (configure_multicast_interface(sockfd, LOCALHOST) == SOCKET_ERROR)
        {
            fprintf(stderr, "Erreur critique : Impossible de configurer une interface réseau. \n");
            closesocket(sockfd);
            WSACleanup();
            exit(EXIT_FAILURE);
        }
        use_fallback = true;
    }

    // Configuration de l'adresse multicast
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin_family = AF_INET;
    multicast_addr.sin_port = htons(MULTICAST_PORT);
    multicast_addr.sin_addr.s_addr = inet_addr(WIFI_INTERFACE_IP);

    if (bind(sockfd, (SOCKADDR *)&multicast_addr, sizeof(multicast_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur de liaison de la socket UDP : %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Socket TCP
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
        fprintf(stderr, "Erreur connexion au système central : %d\n", WSAGetLastError());
        closesocket(sockfd);
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Chauffage connecte au systeme central.\n");

    // Configuration de la requête de jointure au groupe multicast
    multicast_request.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    multicast_request.imr_interface.s_addr = INADDR_ANY; // Utilise l'interface par défaut

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&multicast_request, sizeof(multicast_request)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur de jointure au groupe multicast : %d\n", WSAGetLastError());
        closesocket(sockfd);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    else
    {
        printf("Rejoint le groupe multicast : %s\n", MULTICAST_GROUP);
    }

    printf("Rejoint le groupe multicast %s sur le port %d avec succes.\n", MULTICAST_GROUP, MULTICAST_PORT);

    identifier_chauffage(sockfd, tcp_socket, multicast_addr);

    while (true)
    {
        printf("En attente de Requetes de Chauffage du Syteme central........\n");
        recevoir_commandes_chauffage(tcp_socket, sockfd, multicast_addr);
        // Pause de 5 secondes avant la prochaine itération
        Sleep(5000);
    }

    // Fermeture de la socket et nettoyage
    closesocket(sockfd);
    WSACleanup();
    return 0;
}

// Reception des commandes du syst central
void recevoir_commandes_chauffage(SOCKET tcp_socket, SOCKET sockfd, SOCKADDR_IN multicast_addr)
{
    uint8_t buffer[256];
    MessageTemperature msg;

    int bytes_recus = recv(tcp_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_recus > 0)
    {
        deserializeMessage(buffer, bytes_recus, &msg);

        if (msg.type == MESSAGE_TYPE_CHAUFFER)
        {
            Piece *piece = trouver_ou_ajouter_piece(msg.piece);
            if (piece)
            {
                piece->chauffage_puissance = msg.valeur;
                printf("Commande recue : Augmenter puissance de chauffage pour %s a %d\n", piece->nom, piece->chauffage_puissance);

                // Envoi au groupe Multicast
                ajuster_temperature(piece, sockfd, multicast_addr);
            }
        }
    }
    else if (bytes_recus == 0)
    {
        printf("En attente\n");
    }
    else
    {
        printf("Erreur lors de la reception des commandes : %d\n", WSAGetLastError());
    }
}

// Fonction pour trouver ou ajouter une nouvelle pièce
Piece *trouver_ou_ajouter_piece(const char *nom)
{
    for (int i = 0; i < nb_pieces; i++)
    {
        if (strcmp(pieces[i].nom, nom) == 0)
        {
            return &pieces[i];
        }
    }

    if (nb_pieces < MAX_PIECES)
    {
        strncpy(pieces[nb_pieces].nom, nom, sizeof(pieces[nb_pieces].nom) - 1);
        pieces[nb_pieces].chauffage_puissance = 0;
        pieces[nb_pieces].temperature = 10.0;
        printf("Nouvelle pièce ajoutee : %s\n", nom);
        return &pieces[nb_pieces++];
    }

    printf("Nombre maximum de pieces atteint. Impossible d'ajouter : %s\n", nom);
    return NULL;
}

// ajuste temperature lorsqu'on recoit une commande de Chauffage de la part du system central

void ajuster_temperature(Piece *piece, SOCKET udp_socket, SOCKADDR_IN multicast_addr)
{
    MessageTemperature msg = {
        .type = MESSAGE_TYPE_CHAUFFER,
        .valeur = piece->chauffage_puissance};
    strncpy(msg.piece, piece->nom, sizeof(msg.piece));
    uint8_t buffer[256];
    size_t length;
    serializeMessage(&msg, buffer, &length);

    // Envoi des données multicast
    if (sendto(udp_socket, buffer, length, 0, (SOCKADDR *)&multicast_addr, sizeof(multicast_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur d'envoi multicast : %d\n", WSAGetLastError());
        closesocket(udp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Commande chauffage envoyee : type=%d, valeur=%d, piece=%s\n",
           msg.type, msg.valeur, msg.piece);
}

void identifier_chauffage(SOCKET udp_socket, SOCKET tcp_socket, SOCKADDR_IN multicast_addr)
{
    uint8_t buffer[BUFFER_SIZE];
    MessageTemperature msg;
    socklen_t addrlen = sizeof(multicast_addr);

    printf("Chauffage en attente de messages multicast pour identification...\n");

    while (true)
    {
        int recvlen = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&multicast_addr, &addrlen);
        if (recvlen > 0)
        {
            buffer[recvlen] = '\0';
            deserializeMessage(buffer, recvlen, &msg);

            printf("PIECE : %s\n", msg.piece);
            printf("Multicast reçcu pour chauffage : pièce=%s, valeur=%d\n", msg.piece, msg.valeur);

            // Send identification to the central system
            snprintf((char *)buffer, sizeof(buffer), "TYPE=CHAUFFAGE;PIECE=%s", msg.piece);
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
        else if (recvlen == SOCKET_ERROR)
        {
            fprintf(stderr, "Erreur reception multicast : %d\n", WSAGetLastError());
            break;
        }
    }
}