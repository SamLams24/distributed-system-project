#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "message_temperature.h"

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "-lpthread")

typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;

#define CENTRAL_SYSTEM_PORT 54321
#define GESTION_CONSOLE_PORT 54322
#define BUFFER_SIZE 1024
#define NOM_MAX 50
#define MAX_PIECES 10

// Structure pour pièce
typedef struct
{
    char nom[NOM_MAX];
    float temperature;
    int chauffage_etat;
    pthread_mutex_t lock;
} Piece;

typedef struct
{
    SOCKET tcp_socket; // Socket TCP
    SOCKET udp_socket; // Socket UDP
} ThreadArgs;

typedef struct {
    char piece[NOM_MAX];        // Nom de la pièce (ex: "Salon")
    SOCKET socket;              // Socket associé
    bool is_chauffage;          // Indique si c'est un chauffage (true) ou un thermomètre (false)
} Appareil;

#define MAX_APPAREILS 20
Appareil appareils[MAX_APPAREILS];
int nb_appareils = 0;
MessageTemperature tableau_messages[MAX_APPAREILS];
int nb_messages = 0;
pthread_mutex_t lock_appareils;
pthread_mutex_t lock_messages;

#define MAX_THERMOMETRES 10

typedef struct {
    SOCKET udp_socket_console;   // Socket UDP pour gestion_console
    SOCKET tcp_server_socket;    // Socket TCP pour thermomètres
    SOCKET sockets_thermometres[MAX_THERMOMETRES]; // Sockets actifs des thermomètres
    int nb_thermometres;         // Nombre de thermomètres connectés
    pthread_mutex_t lock_thermometres; // Mutex pour synchroniser l'accès
} ThreadSockets;

Piece pieces[MAX_PIECES];
int nb_pieces = 0;

// Fonctions
void *manage_connexion(void *arg);
void initialiser_pieces();
void mise_a_jour_piece(const char *nom, float temperature);
void recevoir_message_console_udp(SOCKET udp_socket_console, SOCKET tcp_socket_chauffage);
void relayer_temp_vers_gestion_console(SOCKET udp_socket, const MessageTemperature *msg);

void *thread_serveur_console(void *arg);
void *thread_serveur_thermometre(void *arg);
void enregistrer_appareil(const char *nom, SOCKET socket, bool is_chauffage);
SOCKET rechercher_socket_chauffage(const char *piece);
void ajouter_message_temperature(MessageTemperature msg);

int main()
{

    WSADATA wsData;
    SOCKET tcp_server_socket, tcp_client_socket, udp_local_socket;
    SOCKADDR_IN server_addr, udp_console_addr;
    MessageTemperature msg;
    char buffer[BUFFER_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0)
    {
        fprintf(stderr, "Erreur d'initialisation de Winsock : %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    // Creation socket TCP
    if ((tcp_server_socket = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur lors de la création du socket : %d\n", WSAGetLastError());
        closesocket(tcp_server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Creation socket UDP
    if ((udp_local_socket = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET)
    {
        fprintf(stderr, "Erreur lors de la creation du Socket Tcp ! : %d\n", WSAGetLastError());
        closesocket(udp_local_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }
    //,initalisation de la structure server_addr
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(CENTRAL_SYSTEM_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Accepter les connexions provenant de n'importe quel interface de la machine

    if (bind(tcp_server_socket, (SOCKADDR *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur lors du Binding TCP code : %d\n", WSAGetLastError());
        closesocket(tcp_server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    if (listen(tcp_server_socket, 5) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur lors du Listen. Code : %d\n", WSAGetLastError());
        closesocket(tcp_server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    // Configuration de l'adresse UDP pour gestion_console
    memset(&udp_console_addr, 0, sizeof(udp_console_addr));
    udp_console_addr.sin_family = AF_INET;
    udp_console_addr.sin_port = htons(GESTION_CONSOLE_PORT); // Port dédié pour gestion_console
    udp_console_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp_local_socket, (SOCKADDR *)&udp_console_addr, sizeof(udp_console_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur lors du Binding UDP code : %d\n", WSAGetLastError());
        closesocket(tcp_server_socket);
        WSACleanup();
        exit(EXIT_FAILURE);
    }

    printf("Configurations pour communication locales terminés ....\n");
    printf("Configurations Serveur Termine !\n");
    printf("Serveur en attente de connexions sur le port %d.....\n", CENTRAL_SYSTEM_PORT);

    // thread pour gérer les messages UDP de gestion_console
    pthread_t udp_thread;
    SOCKET *udp_socket_ptr = malloc(sizeof(SOCKET));
    *udp_socket_ptr = udp_local_socket;
    pthread_create(&udp_thread, NULL, (void *(*)(void *))recevoir_message_console_udp, udp_socket_ptr);

    // Boucle d'execution
    int iteration = 0;
    while (true)
    {
        SOCKADDR_IN client_addr;
        int addr_len = sizeof(client_addr);

        SOCKET client_socket = accept(tcp_server_socket, (SOCKADDR *)&client_addr, &addr_len);

        if (client_socket == INVALID_SOCKET)
        {
            fprintf(stderr, "Erreur lors de l'acceptation de connexion TCP : %d\n", WSAGetLastError());
            continue;
        }

        printf("Connexion acceptée d'un Client(thermo ou chauff) !\n");

        iteration = 0;
        printf("Connexion acceptee ! \n");
        recevoir_message_console_udp(udp_local_socket, tcp_client_socket);

        ThreadArgs *args = malloc(sizeof(ThreadArgs));
        args->tcp_socket = client_socket;
        args->udp_socket = udp_local_socket;

        pthread_t thread_id;
        pthread_create(&thread_id, NULL, manage_connexion, args);
        pthread_detach(thread_id);
    }

    closesocket(tcp_server_socket);
    WSACleanup();
    return 0;
}

// // Initialiser les pièces
void initialiser_pieces()
{
    char *noms_par_defaut[MAX_PIECES] = {"Piece01", "Piece02", "Piece03", "Piece04", "Piece05"};
    for (int i = 0; i < nb_pieces; i++)
    {
        strncpy(pieces[i].nom, noms_par_defaut[i], NOM_MAX - 1);
        pieces[i].nom[NOM_MAX - 1] = '\0';
        pieces[i].temperature = 10.0; // Une température par défaut
        pieces[i].chauffage_etat = 0; // Aucun chauffage
        pthread_mutex_init(&pieces[i].lock, NULL);
    }
    nb_pieces = 5; // Par défaut
    printf("%d pieces initialises avec succes \n", nb_pieces);
}

// Gerer les connexions externes
void *manage_connexion(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    SOCKET client_socket = args->tcp_socket;
    SOCKET udp_socket = args->udp_socket;
    free(arg);

    if (udp_socket == INVALID_SOCKET)
    {
        fprintf(stderr, "Socket UDP invalide dans le thread manage_connexion.\n");
        return NULL;
    }

    char buffer[BUFFER_SIZE];
    int bytes_recus;
    MessageTemperature msg;

    while ((bytes_recus = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0)
    {
        deserializeMessage(buffer, bytes_recus, &msg);
        if (!validateMessage(&msg))
        {
            fprintf(stderr, "Message non valide : type=%d, valeur=%d, piece=%s\n", msg.type, msg.valeur, msg.piece);
            continue;
        }

        printf("Donnees recu : type=%d, valeur-temperature =%d, piece=%s\n",
               msg.type, msg.valeur, msg.piece);

        printf("Nom : %s, Temperature : %d\n", msg.piece, msg.valeur);

        // On appelle mise a jour temp si le message est de type MESURE(0)
        mise_a_jour_piece(msg.piece, msg.valeur);

        // On relaie les données de température à gestion_console
        relayer_temp_vers_gestion_console(udp_socket, &msg);

        // Réponse au client (facultatif)
        snprintf(buffer, BUFFER_SIZE, "Piece %s mise a jour avec %.2f°C.\n", msg.piece, (float)msg.valeur);
        send(client_socket, buffer, strlen(buffer), 0);
    }

    // Gestion des déconnexions ou erreurs
    if (bytes_recus == 0)
    {
        printf("Le client s'est deconnecte.\n");
    }
    else if (bytes_recus == -1)
    {
        printf("Erreur lors de la reception des donnees : %d\n", WSAGetLastError());
    }

    printf("Connexion fermee ! \n");
    closesocket(client_socket);
    return NULL;
}

// Mise a jour données de piece
void mise_a_jour_piece(const char *nom, float temperature)
{
    bool piece_trouvee = false;

    pthread_mutex_t global_lock;
    pthread_mutex_init(&global_lock, NULL);

    pthread_mutex_lock(&global_lock); // Verrouiller l'accès global au tableau de pièces

    for (int i = 0; i < nb_pieces; i++)
    {
        if (strcmp(pieces[i].nom, nom) == 0)
        {
            pthread_mutex_lock(&pieces[i].lock);

            pieces[i].temperature = temperature;
            printf("Piece %s : Temperature mise a jour a %.2f\n", nom, temperature);

            pthread_mutex_unlock(&pieces[i].lock);
            piece_trouvee = true;
            break;
        }
    }

    if (!piece_trouvee)
    {
        if (nb_pieces < MAX_PIECES)
        {
            // Ajouter une nouvelle pièce
            strncpy(pieces[nb_pieces].nom, nom, NOM_MAX - 1);
            pieces[nb_pieces].nom[NOM_MAX - 1] = '\0';
            pieces[nb_pieces].temperature = temperature;
            pieces[nb_pieces].chauffage_etat = 0;
            pthread_mutex_init(&pieces[nb_pieces].lock, NULL);
            printf("Nouvelle piece ajoutee : %s avec temperature %.2f\n", nom, temperature);
            nb_pieces++;
        }
        else
        {
            printf("Erreur : impossible d'ajouter une nouvelle piece, tableau plein !\n");
        }
    }

    pthread_mutex_unlock(&global_lock); // Déverrouiller l'accès global
    pthread_mutex_destroy(&global_lock);
}

void nettoyer_pieces()
{
    for (int i = 0; i < nb_pieces; i++)
    {
        pthread_mutex_destroy(&pieces[i].lock);
    }
}

// Bon nouvelle methode pour recevoir les commandes de la console

void recevoir_message_console_udp(SOCKET udp_socket_console) {
    char buffer[BUFFER_SIZE];
    SOCKADDR_IN console_addr;
    int console_addr_len = sizeof(console_addr);
    MessageTemperature msg;

    printf("En attente des messages de gestion_console via UDP...\n");

    while (true) {
        int bytes_recus = recvfrom(udp_socket_console, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&console_addr, &console_addr_len);
        if (bytes_recus > 0) {
            buffer[bytes_recus] = '\0';
            deserializeMessage((uint8_t *)buffer, bytes_recus, &msg);

            if (!validateMessage(&msg)) {
                fprintf(stderr, "Message non valide reçu de gestion_console.\n");
                continue;
            }

            printf("Commande reçue : pièce=%s, type=%d, valeur=%d\n", msg.piece, msg.type, msg.valeur);

            if (msg.type == MESSAGE_TYPE_CHAUFFER) {
                SOCKET chauffage_socket = trouver_chauffage_par_piece(msg.piece);
                if (chauffage_socket != INVALID_SOCKET) {
                    if (send(chauffage_socket, buffer, bytes_recus, 0) == SOCKET_ERROR) {
                        fprintf(stderr, "Erreur d'envoi au chauffage : %d\n", WSAGetLastError());
                    } else {
                        printf("Commande envoyée au chauffage de la pièce : %s\n", msg.piece);
                    }
                } else {
                    printf("Aucun chauffage trouvé pour la pièce : %s\n", msg.piece);
                }
            }
        }
    }
}


void relayer_temp_vers_gestion_console(SOCKET udp_socket, const MessageTemperature *msg)
{
    SOCKADDR_IN gestion_console_addr;
    gestion_console_addr.sin_family = AF_INET;
    gestion_console_addr.sin_port = htons(GESTION_CONSOLE_PORT);
    gestion_console_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Adresse locale de gestion_console

    char buffer[BUFFER_SIZE];
    size_t length;
    serializeMessage(msg, (uint8_t *)buffer, &length);

    if (sendto(udp_socket, buffer, length, 0, (SOCKADDR *)&gestion_console_addr, sizeof(gestion_console_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Erreur lors du relai des données vers gestion_console : %d\n", WSAGetLastError());
    }
    else
    {
        printf("Données relayées à gestion_console : type=%d, valeur=%d, pièce=%s\n", msg->type, msg->valeur, msg->piece);
    }
}

void *thread_gestion_console(void *arg) {
    ThreadSockets *sockets = (ThreadSockets *)arg;
    SOCKET udp_socket = sockets->udp_socket_console;

    char buffer[BUFFER_SIZE];
    SOCKADDR_IN console_addr;
    int console_addr_len = sizeof(console_addr);
    MessageTemperature msg;

    while (true) {
        int bytes_recus = recvfrom(udp_socket, buffer, sizeof(buffer) - 1, 0, (SOCKADDR *)&console_addr, &console_addr_len);
        if (bytes_recus > 0) {
            buffer[bytes_recus] = '\0';
            deserializeMessage((uint8_t *)buffer, bytes_recus, &msg);

            if (msg.type == MESSAGE_TYPE_CHAUFFER) {
                printf("Commande chauffage reçue : pièce=%s, puissance=%d\n", msg.piece, msg.valeur);

                // Envoyer la commande à tous les thermomètres connectés
                pthread_mutex_lock(&sockets->lock_thermometres);
                for (int i = 0; i < sockets->nb_thermometres; i++) {
                    SOCKET thermometre_socket = sockets->sockets_thermometres[i];
                    if (send(thermometre_socket, buffer, bytes_recus, 0) == SOCKET_ERROR) {
                        fprintf(stderr, "Erreur d'envoi au thermomètre : %d\n", WSAGetLastError());
                    } else {
                        printf("Commande envoyée au thermomètre : pièce=%s, puissance=%d\n", msg.piece, msg.valeur);
                    }
                }
                pthread_mutex_unlock(&sockets->lock_thermometres);
            }
        }
    }
    return NULL;
}


void *thread_thermometres(void *arg) {
    ThreadSockets *sockets = (ThreadSockets *)arg;
    SOCKET tcp_server_socket = sockets->tcp_server_socket;

    while (true) {
        SOCKADDR_IN client_addr;
        int addr_len = sizeof(client_addr);

        SOCKET client_socket = accept(tcp_server_socket, (SOCKADDR *)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "Erreur lors de l'acceptation : %d\n", WSAGetLastError());
            continue;
        }

        // Ajouter la socket à la liste des thermomètres connectés
        pthread_mutex_lock(&sockets->lock_thermometres);
        if (sockets->nb_thermometres < MAX_THERMOMETRES) {
            sockets->sockets_thermometres[sockets->nb_thermometres++] = client_socket;
            printf("Thermomètre connecté (total : %d).\n", sockets->nb_thermometres);
        } else {
            fprintf(stderr, "Nombre maximum de thermomètres atteint.\n");
            closesocket(client_socket);
        }
        pthread_mutex_unlock(&sockets->lock_thermometres);

        // Traitement des messages reçus
        char buffer[BUFFER_SIZE];
        int bytes_recus;
        while ((bytes_recus = recv(client_socket, buffer, BUFFER_SIZE - 1, 0)) > 0) {
            MessageTemperature msg;
            deserializeMessage((uint8_t *)buffer, bytes_recus, &msg);

            if (msg.type == MESSAGE_TYPE_MESURE) {
                printf("Température reçue : pièce=%s, valeur=%d\n", msg.piece, msg.valeur);

                // Mettre à jour la température de la pièce
                mise_a_jour_piece(msg.piece, (float)msg.valeur);

                // Relayer la température à gestion_console
                relayer_temp_vers_gestion_console(sockets->udp_socket_console, &msg);
            }
        }

        // Supprimer la socket si le thermomètre se déconnecte
        pthread_mutex_lock(&sockets->lock_thermometres);
        for (int i = 0; i < sockets->nb_thermometres; i++) {
            if (sockets->sockets_thermometres[i] == client_socket) {
                sockets->sockets_thermometres[i] = sockets->sockets_thermometres[--sockets->nb_thermometres];
                break;
            }
        }
        pthread_mutex_unlock(&sockets->lock_thermometres);

        closesocket(client_socket);
    }
    return NULL;
}

void enregistrer_appareil(const char *piece, SOCKET socket, bool is_chauffage) {
    pthread_mutex_lock(&lock_appareils);

    if (nb_appareils < MAX_APPAREILS) {
        strncpy(appareils[nb_appareils].piece, piece, NOM_MAX - 1);
        appareils[nb_appareils].piece[NOM_MAX - 1] = '\0';
        appareils[nb_appareils].socket = socket;
        appareils[nb_appareils].is_chauffage = is_chauffage;
        nb_appareils++;
        printf("Appareil enregistré : pièce=%s, type=%s\n", piece, is_chauffage ? "Chauffage" : "Thermomètre");
    } else {
        printf("Erreur : nombre maximum d'appareils atteint.\n");
        closesocket(socket);
    }

    pthread_mutex_unlock(&lock_appareils);
}

SOCKET trouver_chauffage_par_piece(const char *piece) {
    pthread_mutex_lock(&lock_appareils);

    for (int i = 0; i < nb_appareils; i++) {
        if (strcmp(appareils[i].piece, piece) == 0 && appareils[i].is_chauffage) {
            pthread_mutex_unlock(&lock_appareils);
            return appareils[i].socket;
        }
    }

    pthread_mutex_unlock(&lock_appareils);
    return INVALID_SOCKET; // Chauffage non trouvé
}

void *thread_connexions(void *arg) {
    SOCKET tcp_server_socket = *((SOCKET *)arg);
    free(arg);

    while (true) {
        SOCKADDR_IN client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client_socket = accept(tcp_server_socket, (SOCKADDR *)&client_addr, &addr_len);

        if (client_socket == INVALID_SOCKET) {
            fprintf(stderr, "Erreur lors de l'acceptation : %d\n", WSAGetLastError());
            continue;
        }

        printf("Connexion acceptée.\n");

        // Recevoir les informations de l'appareil
        char buffer[BUFFER_SIZE];
        int bytes_recus = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_recus > 0) {
            buffer[bytes_recus] = '\0';

            // Le client envoie son type et le nom de sa pièce (ex: "chauffage Salon")
            char type[20], piece[NOM_MAX];
            sscanf(buffer, "%s %s", type, piece);

            bool is_chauffage = (strcmp(type, "chauffage") == 0);
            enregistrer_appareil(piece, client_socket, is_chauffage);
        } else {
            printf("Erreur lors de la réception des informations du client.\n");
            closesocket(client_socket);
        }
    }
    return NULL;
}


