#include <Arduino.h>
#include <unity.h>

#include "crypto_utils.h"

#include <AES.h>
#include <CBC.h>
#include <ArduinoJson.h>

void testNearestBlockSize(void)
{
    size_t result = find_nearest_block_size(18, 16);
    size_t result2 = find_nearest_block_size(145, 16);
    size_t result3 = find_nearest_block_size(245, 16);
    size_t result4 = find_nearest_block_size(502, 16);
    TEST_ASSERT_EQUAL(32, result);
    TEST_ASSERT_EQUAL(160, result2);
    TEST_ASSERT_EQUAL(256, result3);
    TEST_ASSERT_EQUAL(512, result4);
}

void testApplyPkcs7(void)
{
    byte data[] = {0x01, 0x02, 0x03, 0x03, 0xF3, 0xD3, 0xC3, 0x03, 0x03, 0xA3, 0x03};
    size_t originalSize = sizeof(data);
    size_t adjustedSize = 16; //
    byte preprocessed_data[adjustedSize];

    apply_pkcs7(data, preprocessed_data, originalSize, adjustedSize);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(data, preprocessed_data, originalSize);
    for (size_t i = originalSize; i < adjustedSize; ++i)
    {
        TEST_ASSERT_EQUAL_UINT8(preprocessed_data[i], adjustedSize - originalSize);
    }
}

void testIVGeneration(void)
{

    byte iv[16];
    generate_IV(iv);

    for (int i = 0; i < 16; i++)
    {
        TEST_ASSERT_TRUE(iv[i] >= 0 && iv[i] <= 255);
    }
}

void testEncryption(void)
{
    CBC<AES256> aes;

    byte iv[16] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30};
    byte key[32] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C, 0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
    // The expected cipher for encrypting "dataToEncrypt" was obtained using iCure's encryption function.
    byte expectedCipher[] = {0x89, 0x04, 0x06, 0x56, 0xA9, 0xB4, 0x62, 0x1C, 0xED, 0x1D, 0x13, 0xDF, 0x60, 0x74, 0xF2, 0xE2, 0xE8, 0xF8, 0xDD, 0xBA, 0x62, 0x12, 0x17, 0x3B, 0xFB, 0x1C, 0xD0, 0x88, 0x8E, 0xE7, 0xEF, 0x6C};

    String dataToEncrypt = "test the encryption";
    size_t orignalDataSize = dataToEncrypt.length(); // + 1 for null terminated
    size_t adjustedSize = find_nearest_block_size(orignalDataSize, 16);

    byte plaintext[orignalDataSize];
    byte preprocessed[adjustedSize]; // 16 bytes added by pkcs7
    byte decrypted[orignalDataSize];

    dataToEncrypt.getBytes(plaintext, orignalDataSize + 1);

    apply_pkcs7(plaintext, preprocessed, orignalDataSize, adjustedSize);

    byte ciphertext[adjustedSize];

    encrypt_CBC(&aes, key, adjustedSize, iv, preprocessed, ciphertext, 16); // AES BLOCK SIZE = 16

    TEST_ASSERT_EQUAL_MEMORY(expectedCipher, ciphertext, adjustedSize);

    delay(100);
    decrypt_CBC(&aes, key, adjustedSize, iv, ciphertext, decrypted, 16);

    TEST_ASSERT_EQUAL_MEMORY(decrypted, plaintext, orignalDataSize);
}

void testIsUuidNotEmpty(void)
{
    byte uuidNotEmpty[] = {0x01, 0x02, 0x03};
    size_t sizeNotEmpty = sizeof(uuidNotEmpty);
    TEST_ASSERT_TRUE(!isArrayEmpty(uuidNotEmpty, sizeNotEmpty));

    byte uuidEmpty[] = {0x00, 0x00, 0x00};
    size_t sizeEmpty = sizeof(uuidEmpty);
    TEST_ASSERT_FALSE(!isArrayEmpty(uuidEmpty, sizeEmpty));
}

void setup()
{

    UNITY_BEGIN();
    RUN_TEST(testNearestBlockSize);
    RUN_TEST(testApplyPkcs7);
    RUN_TEST(testEncryption);
    RUN_TEST(testIsUuidNotEmpty);
    RUN_TEST(testIVGeneration);
    UNITY_END();
}

void loop()
{
}