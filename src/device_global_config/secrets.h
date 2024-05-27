#ifndef SECRETS_H
#define SECRETS_H

// #define ICURE_MQTT_ID "ESP32-iCure"
// #define ICURE_MQTT_USER "iCureIoTUser"
// #define ICURE_MQTT_PASSWORD "iCureIoTPassword"

// #define SECRET_SSID "decodeur"
// #define SECRET_PASS "Abcd0714"
#define SECRET_SSID "TaktikWifi"
#define SECRET_PASS "LagoMaggiore"

// #define BROKER_IP "buchinn.be"
// #define BROKER_PORT 1883
// #define TOPIC "icure_nano_topic/sub_topic"

// #define SECRET_SSID "WiFi-2.4-6364"
// #define SECRET_PASS "k2K74xT2vhCU"
// #define LOCAL_BROKER "192.168.1.46"

#define USE_PACKET_ENCRYPTION true

// TO BE MOVED IN BLE_CONSTANTS_H
#define BLE_SVC_UUID "b1be5923-8ca2-415c-9f20-69023f8b4c33"

#define BLE_SENDER_ID_UUID "3dbd6a55-fcc7-4cd8-811f-2c5754296a0a"

#define BLE_CONTACT_ID "477f3318-3a09-4c09-8273-bfb28163b7fd"

#define BLE_TOPIC_UUID "012797b1-152b-4148-92a4-d14e1ee896dc"

#define BLE_SENDER_TOKEN_UUID "0fd7161a-c2e2-44cb-a5ee-9dd3187424fc"

#define BLE_TEST_UUID "8bbc0c8b-d41b-4fd3-8854-af19317d62a1"

#define BLE_KEY_UUID "7626adb2-28ab-4327-8ead-bb571cb1d7f0"
// uuid of the encryption key characteristic
#define BLE_STATUS_UUID "5fae4e14-ca8f-41d7-bd5b-c3a0498973ae"
// uuid of the status characteristic (to know if device has been succesfuly configured)

#endif