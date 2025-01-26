#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <pthread.h>
#include <ws2tcpip.h>
#include "message_temperature.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "-lpthread")

#define TCP_PORT 54323
#define UDP_PORT 54322
#define BUFFER_SIZE 1024

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

// Structure pour les consoles
typedef struct
{
    SOCKET socket;    // Socket TCP
    SOCKADDR_IN addr; // Adresse UDP pour RMI
    int connected;    // Statut de connexion
} Console;

Console console_typeC = {0};
Console console_typeE = {0};
Console console_rmi = {0};

pthread_mutex_t console_mutex = PTHREAD_MUTEX_INITIALIZER;

// Prototypes
void *thread_tcp(void *tcp_socket);
void *thread_udp(void *udp_socket);
void *gerer_console_tcp(void *client_socket);
void enregistrer_console(const char *type, SOCKET socket, SOCKADDR_IN *addr);
void relayer_temperature(const MessageTemperature *msg);
void traiter_commande(const MessageTemperature *msg);

int main()
{
    WSADATA wsaData;
    SOCKET tcp_socket, udp_socket;
    SOCKADDR_IN server_addr;
    pthread_t thread_id;

    // Initialisation Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Erreur Winsock : %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Création des sockets TCP et UDP
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (tcp_socket == INVALID_SOCKET || udp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur création socket : %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Configuration TCP
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_socket, (SOCKADDR *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR ||
        listen(tcp_socket, 5) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur bind/listen TCP : %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    // Configuration UDP
    server_addr.sin_port = htons(UDP_PORT);
    if (bind(udp_socket, (SOCKADDR *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur bind UDP : %d\n", WSAGetLastError());
        return EXIT_FAILURE;
    }

    printf("Gestion Console en attente de connexions TCP (port %d) et UDP (port %d)...\n", TCP_PORT, UDP_PORT);

    // Threads pour TCP et UDP
    pthread_create(&thread_id, NULL, thread_tcp, (void *)&tcp_socket);
    pthread_create(&thread_id, NULL, thread_udp, (void *)&udp_socket);

    pthread_join(thread_id, NULL); // Attente des threads (infinie)
    closesocket(tcp_socket);
    closesocket(udp_socket);
    WSACleanup();
    return 0;
}

// ** Thread TCP principal **
void *thread_tcp(void *tcp_socket)
{
    SOCKET server_socket = *(SOCKET *)tcp_socket;
    SOCKADDR_IN client_addr;
    int addr_len = sizeof(client_addr);
    pthread_t thread_id;

    while (1)
    {
        SOCKET client_socket = accept(server_socket, (SOCKADDR *)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET)
        {
            fprintf(stderr, "Erreur accept : %d\n", WSAGetLastError());
            continue;
        }

        printf("Nouvelle connexion TCP acceptee.\n");

        // Créer un thread pour gérer la console connectée
        SOCKET *client_socket_ptr = malloc(sizeof(SOCKET));
        *client_socket_ptr = client_socket;
        pthread_create(&thread_id, NULL, gerer_console_tcp, (void *)client_socket_ptr);
        pthread_detach(thread_id);
    }
    return NULL;
}

// ** Gestion des consoles TCP (typeC ou typeE) **
void *gerer_console_tcp(void *client_socket)
{
    SOCKET socket = *(SOCKET *)client_socket;
    free(client_socket);

    char buffer[BUFFER_SIZE];
    MessageTemperature msg;

    // Identification de la console
    int recvlen = recv(socket, buffer, BUFFER_SIZE - 1, 0);
    if (recvlen > 0)
    {
        buffer[recvlen] = '\0';
        deserializeMessage(buffer, recvlen, &msg);
        if (strncmp(msg.piece, "ConsoleTYPE=C", 13) == 0)
        {
            enregistrer_console("C", socket, NULL);
        }
        else if (strncmp(msg.piece, "ConsoleTYPE=E", 13) == 0)
        {
            enregistrer_console("E", socket, NULL);
        }
        else
        {
            printf("Type de console inconnu : %s\n", buffer);
            closesocket(socket);
            return NULL;
        }
    }

    // Traitement des messages après identification
    while ((recvlen = recv(socket, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        buffer[recvlen] = '\0';
        deserializeMessage(buffer, recvlen, &msg);

        if (msg.type == MESSAGE_TYPE_CHAUFFER)
        {
            traiter_commande(&msg); // Relayer les commandes vers communication_temperature
        }
    }

    printf("Console déconnectée.\n");
    closesocket(socket);
    return NULL;
}

// ** Thread UDP principal (typeRMI ou communication_temperature) **
void *thread_udp(void *udp_socket)
{
    SOCKET socket = *(SOCKET *)udp_socket;
    SOCKADDR_IN sender_addr;
    int sender_len = sizeof(sender_addr);
    char buffer[BUFFER_SIZE];
    MessageTemperature msg;

    while (1)
    {
        int recvlen = recvfrom(socket, buffer, BUFFER_SIZE - 1, 0, (SOCKADDR *)&sender_addr, &sender_len);
        if (recvlen > 0)
        {
            buffer[recvlen] = '\0';
            deserializeMessage(buffer, recvlen, &msg);

            if (strncmp(buffer, "IDENTIFICATION;TYPE=RMI", 23) == 0)
            {
                enregistrer_console("RMI", INVALID_SOCKET, &sender_addr);
            }
            else if (msg.type == MESSAGE_TYPE_MESURE)
            {
                relayer_temperature(&msg); // Relayer aux consoles typeE et RMI
            }
        }
    }
    return NULL;
}

// ** Enregistrer une console **
void enregistrer_console(const char *type, SOCKET socket, SOCKADDR_IN *addr)
{
    pthread_mutex_lock(&console_mutex);

    if (strcmp(type, "C") == 0)
    {
        console_typeC.socket = socket;
        console_typeC.connected = 1;
        printf("Console Type C enregistree.\n");
    }
    else if (strcmp(type, "E") == 0)
    {
        console_typeE.socket = socket;
        console_typeE.connected = 1;
        printf("Console Type E enregistree.\n");
    }
    else if (strcmp(type, "RMI") == 0 && addr != NULL)
    {
        console_rmi.addr = *addr;
        console_rmi.connected = 1;
        printf("Console RMI enregistree.\n");
    }

    pthread_mutex_unlock(&console_mutex);
}

// ** Relayer les températures vers Type E et RMI **
void relayer_temperature(const MessageTemperature *msg)
{
    pthread_mutex_lock(&console_mutex);

    char buffer[BUFFER_SIZE];
    size_t length;
    serializeMessage(msg, (uint8_t *)buffer, &length);

    if (console_typeE.connected)
    {
        send(console_typeE.socket, buffer, length, 0);
        printf("Température relayée à Console Type E.\n");
    }
    if (console_rmi.connected)
    {
        sendto(console_rmi.socket, buffer, length, 0, (SOCKADDR *)&console_rmi.addr, sizeof(console_rmi.addr));
        printf("Température relayée à Console RMI.\n");
    }

    pthread_mutex_unlock(&console_mutex);
}

///  Traiter les commandes de chauffage 
void traiter_commande(const MessageTemperature *msg)
{
    printf("Relais de commande vers communication_temperature : type=%d, valeur=%d, pièce=%s\n",
           msg->type, msg->valeur, msg->piece);

    // Configurer l'adresse de communication_temperature
    SOCKADDR_IN com_temp_addr;
    memset(&com_temp_addr, 0, sizeof(com_temp_addr));
    com_temp_addr.sin_family = AF_INET;
    com_temp_addr.sin_port = htons(54321); // Port de communication_temperature
    com_temp_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Adresse locale de communication_temperature

    // Créer un socket UDP pour envoyer les messages
    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur de création du socket UDP : %d\n", WSAGetLastError());
        return;
    }

    // Sérialiser le message
    char buffer[BUFFER_SIZE];
    size_t length;
    serializeMessage(msg, (uint8_t *)buffer, &length);

    // Envoyer le message à communication_temperature
    int sent_len = sendto(udp_socket, buffer, length, 0, (SOCKADDR *)&com_temp_addr, sizeof(com_temp_addr));
    if (sent_len == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur d'envoi du message a communication_temperature : %d\n", WSAGetLastError());
    }
    else
    {
        printf("Message envoye a communication_temperature : type=%d, valeur=%d, pièce=%s\n",
               msg->type, msg->valeur, msg->piece);
    }

    // Fermer le socket UDP
    closesocket(udp_socket);
}

