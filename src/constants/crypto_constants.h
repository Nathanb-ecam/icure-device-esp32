#ifndef CRYPTO_CONSTANTS_H
#define CRYPTO_CONSTANTS_H

#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE 32
#define IV_SIZE 16

#include <constants/mqtt_constants.h>

struct Encryption_Data
{
    byte iv[16];
    byte plaintext[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE - 32];
    byte plaintext_with_padding[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE - 16]; // we remove 16 because padding may add up to 16 and IV adds 16
    byte ciphertext[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE - 16];
    size_t adjustedSize;                                      // plaintext size + padding
    byte encryptedSelf[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE]; // cipher prepended with the IV (size of the cipher +16)
    size_t encryptedSelfSize;                                 // adjusted size + IV size
    size_t encryptedSelfBase64Size;
    int base64EncodeLength;
};

#endif