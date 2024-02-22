#include <crypto_utils.h>

bool isUuidNotEmpty(const byte *uuidArray, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        if (uuidArray[i] != 0)
        {
            return true;
        }
    }
    return false;
}

void printUuid(const byte *uuidArray, size_t size)
{
    for (size_t i = 0; i < size; ++i)
    {
        Serial.print(uuidArray[i], HEX);
        Serial.print(" ");
    }
    Serial.println();
}

void printHex(const char *name, const byte *data, size_t dataSize)
{
    Serial.println(name);
    for (int i = 0; i < dataSize; i++)
    {
        Serial.print(data[i], HEX);
        Serial.print(" ");
    }
    Serial.println("");
}

String byteToHexString(const byte *uuidArray, size_t size)
{
    String hexString = "";
    for (size_t i = 0; i < size; ++i)
    {
        char hex[3];
        sprintf(hex, "%02X", uuidArray[i]);
        hexString += hex;
    }
    return hexString;
}

void hexStringToBytes(String hexString, byte *bytes)
{
    for (int i = 0; i < hexString.length(); i += 2)
    {
        bytes[i / 2] = (byte)strtol(hexString.substring(i, i + 2).c_str(), NULL, 16);
    }
}

size_t find_nearest_block_size(size_t dataSize, size_t chunk_size)
{
    if (dataSize % chunk_size == 0)
    {
        return dataSize + chunk_size;
    }
    else
    {
        // Round up to the next multiple of 16
        return ((dataSize / chunk_size) + 1) * chunk_size;
    }
}

void apply_pkcs7(byte *data, byte *preprocessed_data, size_t originalSize, size_t adjustedSize)
{ // PKCS7
    memcpy(preprocessed_data, data, originalSize);
    int padding = adjustedSize - originalSize;
    Serial.print("[INFO] Pkcs7 : ");
    Serial.println(padding);

    byte bytePadding = static_cast<byte>(padding);

    memset(preprocessed_data + originalSize, bytePadding, padding);
    Serial.println();
}

void encrypt_CBC(Cipher *cipher, byte *key, size_t textSize, byte *iv, byte *dataToEncrypt, byte *ciphertext, size_t inc)
{
    size_t posn, len;

    cipher->clear();
    if (!cipher->setKey(key, cipher->keySize()))
    {
        Serial.print("setKey error");
    }
    if (!cipher->setIV(iv, cipher->ivSize()))
    {
        Serial.print("setIV error");
    }

    memset(ciphertext, 0xBA, sizeof(ciphertext));

    for (posn = 0; posn < textSize; posn += inc)
    {
        len = textSize - posn;
        if (len > inc)
            len = inc;
        cipher->encrypt(ciphertext + posn, dataToEncrypt + posn, len);
    }

    // Serial.println("------------------------------");
    // DEBUG
    // printHex("Encrypted", ciphertext, textSize);
}

void decrypt_CBC(Cipher *cipher, byte *key, size_t textSize, byte *iv, byte *ciphertext, byte *decryptedtext, size_t inc)
{
    size_t posn, len;
    cipher->clear();

    if (!cipher->setKey(key, cipher->keySize()))
    {
        Serial.println("setKey failed");
    }

    if (!cipher->setIV(iv, cipher->ivSize()))
    {
        Serial.println("setIV failed");
    }

    for (posn = 0; posn < textSize; posn += inc)
    {
        len = textSize - posn;
        if (len > inc)
            len = inc;
        cipher->decrypt(decryptedtext + posn, ciphertext + posn, len);
    }

    // printHex("Decrypted", decryptedtext, textSize);
    // Serial.println("------------------------------");
}

void generate_IV(byte *iv)
{
    Serial.print("[INFO] IV : ");
    for (int i = 0; i < 16; i++)
    {
        u_int8_t ran = analogRead(0);
        u_int8_t ran1 = random(256);
        u_int8_t notSoRandom = ran - ran1;
        byte d = static_cast<byte>(notSoRandom);

        String hexString = String(d, HEX);
        if (hexString.length() == 1)
        {
            hexString = "0" + hexString;
        }

        iv[i] = (byte)strtol(hexString.c_str(), NULL, 16);
        Serial.print(hexString);
    }
    Serial.println();
}