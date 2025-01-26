#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include "message_temperature.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "-lpthread")
#pragma comment(lib, "-lws2_32")
#pragma comment(lib, "-liphlpapi")

#define UDP_PORT 54322
#define TCP_PORT 54323
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

typedef struct {
    SOCKET udp_socket; // Socket UDP principal (partagé)
    SOCKET tcp_socket; // Socket TCP client (unique par client)
} ThreadData;

void *gerer_tcp_connection(void *args);
void *gerer_udp_connexion(void *args);

int main() {
    WSADATA wsaData;
    SOCKET tcp_socket, udp_socket;
    SOCKADDR_IN server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    pthread_t thread_id;

    // Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "Erreur initialisation Winsock : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Création de la socket TCP
    tcp_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcp_socket == INVALID_SOCKET) {
        fprintf(stderr, "Erreur création socket TCP : %d\n", WSAGetLastError());
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Création de la socket UDP
    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == INVALID_SOCKET) {
        fprintf(stderr, "Erreur création socket UDP : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configurer l'adresse du serveur pour TCP
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(tcp_socket, (SOCKADDR *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Erreur bind TCP : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        closesocket(udp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configurer l'adresse du serveur pour UDP
    server_addr.sin_port = htons(UDP_PORT);
    if (bind(udp_socket, (SOCKADDR *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        fprintf(stderr, "Erreur bind UDP : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        closesocket(udp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Écoute des connexions TCP
    if (listen(tcp_socket, MAX_CLIENTS) == SOCKET_ERROR) {
        fprintf(stderr, "Erreur listen TCP : %d\n", WSAGetLastError());
        closesocket(tcp_socket);
        closesocket(udp_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Serveur en attente de connexions TCP sur le port %d...\n", TCP_PORT);

    // Gestion des connexions TCP
    while (true) {
        SOCKET client_socket = accept(tcp_socket, (SOCKADDR *)&client_addr, &client_addr_len);
        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "Erreur accept : %d\n", WSAGetLastError());
            continue;
        }
        printf("Nouvelle connexion TCP acceptée.\n");

        // Préparer les données pour le thread
        ThreadData *thread_data = malloc(sizeof(ThreadData));
        if (!thread_data) {
            fprintf(stderr, "Erreur allocation mémoire pour thread_data.\n");
            closesocket(client_socket);
            continue;
        }
        thread_data->tcp_socket = client_socket;
        thread_data->udp_socket = udp_socket; // Passer le socket UDP principal

        // Créer un thread pour gérer cette connexion
        if (pthread_create(&thread_id, NULL, gerer_tcp_connection, thread_data) != 0) {
            fprintf(stderr, "Erreur création thread TCP\n");
            closesocket(client_socket);
            free(thread_data);
        }
    }

    // Fermeture des sockets
    closesocket(tcp_socket);
    closesocket(udp_socket);
    WSACleanup();

    return 0;
}

// Gestion des connexions par TCP
void *gerer_tcp_connection(void *args) {
    ThreadData *data = (ThreadData *)args;
    SOCKET tcp_socket = data->tcp_socket;
    SOCKET udp_socket = data->udp_socket; // Socket UDP partagé
    pthread_t udp_thread_id;
    MessageTemperature msg;

    char buffer[BUFFER_SIZE];
    int recvlen;

    printf("[TCP] Gestion d'une nouvelle connexion client.\n");

    // Créer un thread UDP pour ce client
    if (pthread_create(&udp_thread_id, NULL, gerer_udp_connexion, data) != 0) {
        fprintf(stderr, "Erreur création thread UDP.\n");
        closesocket(tcp_socket);
        free(data);
        return NULL;
    }

    // Boucle de réception TCP
    while ((recvlen = recv(tcp_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
        buffer[recvlen] = '\0';
        printf("[TCP] Message reçu : %s\n", buffer);
        deserializeMessage(buffer, recvlen, &msg);

        printf("Données formatés : type= %d, valeur= %d, piece= %d", msg.type, msg.valeur, msg.piece);
        // Logique de traitement du message TCP
    }

    printf("[TCP] Connexion fermée.\n");
    closesocket(tcp_socket);
    return NULL;
}

// Gestion des messages par UDP
void *gerer_udp_connexion(void *args) {
    ThreadData *data = (ThreadData *)args;
    SOCKET udp_socket = data->udp_socket; // Socket UDP partagé
    SOCKET tcp_socket = data->tcp_socket; // Socket TCP client

    SOCKADDR_IN client_addr;
    int client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    int recvlen;

    printf("[UDP] Gestion des messages pour ce client.\n");

    // Boucle de réception UDP
    while ((recvlen = recvfrom(udp_socket, buffer, BUFFER_SIZE - 1, 0, (SOCKADDR *)&client_addr, &client_addr_len)) > 0) {
        buffer[recvlen] = '\0';
        printf("[UDP] Message reçu : %s\n", buffer);

        // Relayer le message reçu via TCP
        if (send(tcp_socket, buffer, recvlen, 0) == SOCKET_ERROR) {
            fprintf(stderr, "[UDP->TCP] Erreur d'envoi : %d\n", WSAGetLastError());
        } else {
            printf("[UDP->TCP] Message relayé au client TCP.\n");
        }
    }

    printf("[UDP] Fermeture du thread UDP.\n");
    free(data);
    return NULL;
}
