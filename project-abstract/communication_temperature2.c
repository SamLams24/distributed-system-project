#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <winsock2.h>
#include <pthread.h>
#include <ws2tcpip.h>
#include "message_temperature.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "-lpthread")

#define CENTRAL_SYSTEM_PORT 54321
#define GESTION_CONSOLE_PORT 54322
#define BUFFER_SIZE 1024
#define MAX_APPAREILS 50
#define NOM_MAX 50

// Identification des appareils
#define TYPE_THERMOMETRE 1
#define TYPE_CHAUFFAGE 2

// Structures
typedef struct
{
    char piece[NOM_MAX];
    SOCKET socket;
    int type;
} Appareil;

// Variables globales
Appareil appareils[MAX_APPAREILS];
int nb_appareils = 0;

pthread_mutex_t lock_appareils;
pthread_mutex_t udp_mutex = PTHREAD_MUTEX_INITIALIZER;

// Prototypes
void *thread_serveur_principal(void *arg);
void *thread_serveur_thermometre(void *arg);
void *thread_serveur_chauffage(void *arg);
int identifier_appareil(const char *message);
void enregistrer_appareil(const char *piece, SOCKET socket, int type);
void envoyer_udp_securise(SOCKET udp_socket, const char *buffer, size_t length, struct sockaddr_in *addr) ;

int main()
{
    WSADATA wsData;
    pthread_t thread_principal;

    // Initialisation de Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
    {
        fprintf(stderr, "Erreur d'initialisation de Winsock : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Initialisation des mutex
    pthread_mutex_init(&lock_appareils, NULL);

    // Lancer le thread principal
    pthread_create(&thread_principal, NULL, thread_serveur_principal, NULL);

    // Attendre le thread principal
    pthread_join(thread_principal, NULL);

    // Nettoyage
    WSACleanup();
    return 0;
}

// Thread principal pour écouter les connexions
void *thread_serveur_principal(void *arg)
{
    SOCKET server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    int addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    // Création du socket TCP
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur de creation du socket : %d\n", WSAGetLastError());
        pthread_exit(NULL);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CENTRAL_SYSTEM_PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR ||
        listen(server_socket, 5) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur de liaison/écoute : %d\n", WSAGetLastError());
        closesocket(server_socket);
        pthread_exit(NULL);
    }

    printf("Serveur en attente de connexions sur le port %d...\n", CENTRAL_SYSTEM_PORT);

    while ((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len)) != INVALID_SOCKET)
    {
        printf("Nouvelle connexion depuis %s:%d\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        //nettoyage du buffer 
        memset(buffer, 0, sizeof(buffer));       

        // Lire le message d'identification
        int bytes_recus = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_recus > 0)
        {

            buffer[BUFFER_SIZE - 1] = '\0';
            //printf("Message brut reçu : %s\n", buffer);
        }
        else if (bytes_recus == 0)
        {
            printf("Client deconnecte proprement.\n");
        }
        else
        {
            fprintf(stderr, "Erreur de reception : %d\n", WSAGetLastError());
        }
        int type = identifier_appareil(buffer);

        if (type == TYPE_THERMOMETRE || type == TYPE_CHAUFFAGE)
        {
            // Extraction du nom de la pièce
            char piece[NOM_MAX];
            sscanf(buffer, "TYPE=%*[^;];PIECE=%s", piece);

            // Enregistrement de l'appareil
            enregistrer_appareil(piece, client_socket, type);

            // Démarrer le thread correspondant
            pthread_t thread_id;
            SOCKET *socket_ptr = malloc(sizeof(SOCKET));
            *socket_ptr = client_socket;

            if (type == TYPE_THERMOMETRE)
            {
                pthread_create(&thread_id, NULL, thread_serveur_thermometre, socket_ptr);
            }
            else if (type == TYPE_CHAUFFAGE)
            {
                pthread_create(&thread_id, NULL, thread_serveur_chauffage, socket_ptr);
            }
            pthread_detach(thread_id);
        }
        else
        {
            printf("Message d'identification inconnu : %s\n", buffer);
            closesocket(client_socket);
        }
    }

    closesocket(server_socket);
    pthread_exit(NULL);
}

// Thread pour gérer un thermomètre
void *thread_serveur_thermometre(void *arg)
{
    SOCKET client_socket = *(SOCKET *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    struct sockaddr_in console_addr;
    int bytes_recus;

    // Configuration de l'adresse UDP de gestion_console
    console_addr.sin_family = AF_INET;
    console_addr.sin_port = htons(GESTION_CONSOLE_PORT);   // Port de gestion_console (à définir)
    console_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // inet_addr("127.0.0.1"); INADDR_ANY

    SOCKET udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur création socket UDP : %d\n", WSAGetLastError());
        closesocket(client_socket);
        return NULL;
    }

    if (console_addr.sin_addr.s_addr == INADDR_NONE)
    {
        fprintf(stderr, "Erreur : adresse de gestion_console invalide\n");
        closesocket(udp_socket);
        return NULL;
    }

    memset(buffer, 0, sizeof(buffer));

    while (true)
    {

        // Debut traitement
        bytes_recus = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_recus > 0)
        {
            MessageTemperature msg;
            // size_t length = (size_t)sizeof(buffer);
            // printf("Contenu brut du buffer : ");
            // for (size_t i = 0; i < length; i++)
            // {
            //     printf("%02X ", buffer[i]);
            // }
            // printf("\n");
            deserializeMessage(buffer, bytes_recus, &msg);

            if (!validateMessage(&msg))
            {
                fprintf(stderr, "Message non valide : type=%d, valeur=%d, piece=%s\n", msg.type, msg.valeur, msg.piece);
                continue;
            }

            printf("Message recu du thermometre : type=%d, valeur=%d, piece=%s\n",
                   msg.type, msg.valeur, msg.piece);

            // Relayer le message à gestion_console
            if (sendto(udp_socket, buffer, bytes_recus, 0, (struct sockaddr *)&console_addr, sizeof(console_addr)) == SOCKET_ERROR)
            {
                fprintf(stderr, "Erreur d'envoi UDP vers gestion_console : %d\n", WSAGetLastError());
            }
            else
            {
                printf("Message relaye a gestion_console : type=%d, valeur=%d, piece=%s\n",
                       msg.type, msg.valeur, msg.piece);
            }

        }
        else if (bytes_recus == 0)
        {
            // Le client s'est déconnecté proprement
            printf("Client thermometre deconnecte proprement.\n");
            break;
        }
        else
        {
            // Erreur de réception
            fprintf(stderr, "Erreur de réception : %d\n", WSAGetLastError());
            break;
        }
    }
    // Fermeture des sockets
    closesocket(client_socket);
    closesocket(udp_socket);
    printf("Connexion thermomètre terminée.\n");
    return NULL;
}

// Thread pour gérer un chauffage
void *thread_serveur_chauffage(void *arg)
{
    SOCKET client_socket = *(SOCKET *)arg;
    free(arg);

    char buffer[BUFFER_SIZE];
    while (recv(client_socket, buffer, sizeof(buffer) - 1, 0) > 0)
    {
        printf("Chauffage : %s\n", buffer);
        // Traiter les commandes du chauffage
    }

    closesocket(client_socket);
    printf("Connexion chauffage terminee.\n");
    return NULL;
}

// Identifier l'appareil en fonction du message d'identification
int identifier_appareil(const char *message)
{
    char type[BUFFER_SIZE] = {0};
    char piece[NOM_MAX] = {0};

    // Analyse du message
    if (sscanf(message, "TYPE=%[^;];PIECE=%s", type, piece) == 2)
    {
        if (strcmp(type, "THERMOMETRE") == 0)
        {
            printf("Appareil identifie : Thermometre, piece=%s\n", piece);
            return TYPE_THERMOMETRE;
        }
        else if (strcmp(type, "CHAUFFAGE") == 0)
        {
            printf("Appareil identifie : Chauffage, piece=%s\n", piece);
            return TYPE_CHAUFFAGE;
        }
    }
    return -1; // Type inconnu
}

void enregistrer_appareil(const char *piece, SOCKET socket, int type)
{
    pthread_mutex_lock(&lock_appareils);

    if (nb_appareils < MAX_APPAREILS)
    {
        strncpy(appareils[nb_appareils].piece, piece, NOM_MAX - 1);
        appareils[nb_appareils].piece[NOM_MAX - 1] = '\0';
        appareils[nb_appareils].socket = socket;
        appareils[nb_appareils].type = type;

        printf("Appareil enregistre : type=%s, piece=%s, socket=%d\n",
               type == TYPE_THERMOMETRE ? "Thermometre" : "Chauffage",
               piece, socket);

        nb_appareils++;
    }
    else
    {
        printf("Limite d'appareils atteinte, impossible d'enregistrer : %s\n", piece);
    }

    pthread_mutex_unlock(&lock_appareils);
}


