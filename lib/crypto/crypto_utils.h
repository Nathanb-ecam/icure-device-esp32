#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <Arduino.h>
#include <Cipher.h>

bool isArrayEmpty(const byte *uuidArray, size_t size);

void printUuid(const byte *uuidArray, size_t size);

void printHex(const char *name, const byte *data, size_t dataSize);

String byteToHexString(const byte *uuidArray, size_t size);

void hexStringToBytes(String hexString, byte *bytes);

size_t find_nearest_block_size(size_t dataSize, size_t chunk_size);

void apply_pkcs7(byte *data, byte *preprocessed_data, size_t originalSize, size_t adjustedSize);

void encrypt_CBC(Cipher *cipher, byte *key, size_t textSize, byte *iv, byte *dataToEncrypt, byte *ciphertext, size_t inc);

void decrypt_CBC(Cipher *cipher, byte *key, size_t textSize, byte *iv, byte *ciphertext, byte *decryptedtext, size_t inc);

void generate_IV(byte *iv);

#endif