#ifndef ICURE_DEVICE_CONSTANTS_H
#define ICURE_DEVICE_CONSTANTS_H

#include <secrets.h>
#include <constants/crypto_constants.h>
#include <ArduinoBLE.h>

boolean isAdvertising = false;

BLEService icureService(BLE_SVC_UUID);
// toggle led characteristic
BLEStringCharacteristic testCharacteristic(BLE_TEST_UUID, BLEWrite, 5); // android app can try to turn on arduino built in led
// sender uuid : 128 bit
BLEStringCharacteristic senderIdCharacteristic(BLE_SENDER_ID_UUID, BLEWrite, 64);
// sender token
BLECharacteristic senderTokenCharacteristic(BLE_SENDER_TOKEN_UUID, BLEWrite, 16);
// contact id
BLECharacteristic contactIdCharacteristic(BLE_CONTACT_ID, BLEWrite, 16);
// Symmetric key characteristic : 256 bit key
BLECharacteristic keyCharacteristic(BLE_KEY_UUID, BLEWrite, 32);
// TO Notify the android app when all required characteristics are set
BLEByteCharacteristic statusCharacteristic(BLE_STATUS_UUID, BLENotify); // BLERead | BLENotify

struct BLE_Data // VALUES TO RECEIVE FROM ANDROID
{
    byte senderId[32];
    bool senderIdReady = false;
    String senderIdString;

    byte contactId[16];
    String contactIdHexString;
    bool contactIdReady = false;

    byte senderToken[16];
    String senderTokenHexString;
    bool senderTokenReady = false;

    byte encKey[AES_KEY_SIZE];
    String encKeyHexString;
    bool encKeyReady = false;
};

#endif