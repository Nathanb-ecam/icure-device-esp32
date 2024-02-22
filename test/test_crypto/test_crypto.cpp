#include <Arduino.h>
#include <unity.h>
#include "crypto_utils.h"

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

void testEncryptCBC(void)
{
    const size_t textSize = 16;
    const size_t inc = 16;
    byte key[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10,
                  0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};
    byte iv[] = {0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30};
    byte dataToEncrypt[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40};
    byte ciphertext[textSize];
    byte expectedCiphertext[textSize] = {0};

    encrypt_CBC(nullptr, key, textSize, iv, dataToEncrypt, ciphertext, inc);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(expectedCiphertext, ciphertext, textSize);
}

void testIsUuidNotEmpty(void)
{
    byte uuidNotEmpty[] = {0x01, 0x02, 0x03};
    size_t sizeNotEmpty = sizeof(uuidNotEmpty);
    TEST_ASSERT_TRUE(isUuidNotEmpty(uuidNotEmpty, sizeNotEmpty));

    byte uuidEmpty[] = {0x00, 0x00, 0x00};
    size_t sizeEmpty = sizeof(uuidEmpty);
    TEST_ASSERT_FALSE(isUuidNotEmpty(uuidEmpty, sizeEmpty));
}

void setup()
{
    delay(2000);

    UNITY_BEGIN(); // IMPORTANT LINE!
    RUN_TEST(testNearestBlockSize);
    RUN_TEST(testApplyPkcs7);
    // RUN_TEST(testEncryptCBC);
    RUN_TEST(testIsUuidNotEmpty);
    RUN_TEST(testIVGeneration);
    UNITY_END();
}

void loop()
{
}