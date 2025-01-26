#include "message_temperature.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Sérialisation : Convertit la structure en tableau d'octets
void serializeMessage(const MessageTemperature *msg, uint8_t *buffer, size_t *length)
{
    int i;
    int32_t valeur = msg->valeur;

    // Copie de `valeur` (4 octets)
    for (i = 0; i < 4; i++)
    {
        buffer[i] = (uint8_t)(valeur & 0xFF);
        valeur >>= 8;
    }

    // Copie de `type` (1 octet)
    buffer[4] = msg->type;

    // Copie sécurisée de `piece` (chaîne de caractères)
    size_t piece_length = strnlen(msg->piece, sizeof(msg->piece)); // Longueur sécurisée
    if (piece_length > 250)
    { // Par sécurité, éviter de dépasser une limite arbitraire
        fprintf(stderr, "Erreur : chaîne 'piece' trop longue\n");
        return;
    }
    strncpy((char *)(buffer + 5), msg->piece, piece_length);
    buffer[5 + piece_length] = '\0'; // Ajout du terminateur nul

    // Taille totale du message sérialisé
    *length = 5 + piece_length + 1; // +1 pour le terminateur
}

// Désérialisation : Reconstruit la structure à partir d'un tableau d'octets
void deserializeMessage(const uint8_t *buffer, size_t length, MessageTemperature *msg)
{
    // printf("Buffer brut reçu (longueur=%zu): ", length);
    // for (size_t i = 0; i < length; i++) {
    //     printf("%02X ", buffer[i]);
    // }
    // printf("\n");

    if (length < sizeof(uint8_t) + sizeof(int32_t))
    {
        fprintf(stderr, "Erreur : longueur du message insuffisante\n");
        return;
    }
    int i;

    // Reconstruit `valeur` (4 octets)
    msg->valeur = 0;
    for (i = 0; i < 4; i++)
    {
        msg->valeur |= ((int32_t)buffer[i] & 0xFF) << (i * 8);
    }

    // Récupère `type` (1 octet)
    msg->type = buffer[4];

    // Récupère `piece` (chaîne de caractères)
    size_t piece_length = length - 5;
    if (piece_length > sizeof(msg->piece) - 1)
    { // -1 pour le terminateur
        fprintf(stderr, "Erreur : chaîne 'piece' trop longue\n");
        piece_length = sizeof(msg->piece) - 1; // Tronquer si nécessaire
    }
    strncpy(msg->piece, (char *)(buffer + 5), piece_length);
    msg->piece[piece_length] = '\0'; // Ajoute un terminateur nul
}

bool validateMessage(const MessageTemperature *msg)
{
    if (msg->type != MESSAGE_TYPE_MESURE)
    {
        fprintf(stderr, "Type de message invalide : %d\n", msg->type);
        return false;
    }
    /* Je doute que la température ne puisse depasser 100*/
    if (msg->valeur < -100 || msg->valeur > 100)
    {
        fprintf(stderr, "Valeur de température hors limites : %d\n", msg->valeur);
        return false;
    }
    if (strlen(msg->piece) == 0 || strlen(msg->piece) > 255)
    {
        fprintf(stderr, "Nom de pièce invalide : '%s'\n", msg->piece);
        return false;
    }

    return true;
}
