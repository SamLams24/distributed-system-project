#ifndef MESSAGE_TEMPERATURE_H
#define MESSAGE_TEMPERATURE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

// Types de message
#define MESSAGE_TYPE_MESURE 0    
#define MESSAGE_TYPE_CHAUFFER 1  

// Structure du message
typedef struct {
    uint8_t type;        
    int32_t valeur;      
    char piece[256];     
} MessageTemperature;

// Déclarations des fonctionss
void serializeMessage(const MessageTemperature *msg, uint8_t *buffer, size_t *length);
void deserializeMessage(const uint8_t *buffer, size_t length, MessageTemperature *msg);
bool validateMessage(const MessageTemperature *msg);

#endif // MESSAGE_TEMPERATURE_H
