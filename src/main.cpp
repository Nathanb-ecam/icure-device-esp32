#include <Arduino.h>

#include <Base64.h>

#include <DHT.h>
#include <DFRobot_Heartrate.h>

#include <CryptoAES_CBC.h>
#include <AES.h>
#include <CBC.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>

#include <WiFiClientSecure.h>
#include <ArduinoBLE.h>

#include "mqtt_utils.h"
#include "crypto_utils.h"
#include "led_utils.h"
#include "secrets.h"

#include <string.h>

#define INTERVAL_BETWEEN_MQTT_PACKETS 7000 // ms

// Humidity/temperature
#define DHT_TYPE DHT11
#define DHTPIN 34

#define heartratePin A1

#define MQTT_MAX_PACKET_SIZE 1024
#define MAX_AUTH_SIZE 64
// uid, token , cid

// 256 required by mqttLib - 16 from IV
#define AES_BLOCK_SIZE 16
#define AES_KEY_SIZE 32
#define IV_SIZE 16

#define ESP_LED_BUILTIN 2

enum State
{
    INIT,
    BLUETOOTH_KEY_CONFIG,
    SETUP_MQTT,
    READ_SENSORS_DATA,
    ENCRYPT_DATA,
    PREPARE_MQTT_PACKET,
    HANDLE_WAIT,
    SENDING_DATA,
    ERROR
};

/* BLUETOOTH INIT */

bool builtinLedOn = false;
struct BLE_Data // VALUES TO RECEIVE FROM ANDROID
{
    // byte senderUuid[16];
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

struct WIFI_Config
{
    const char *ssid = SECRET_SSID; // your network SSID (name)
    const char *pass = SECRET_PASS;
};

struct Broker_Config
{
    const char *IP = LOCAL_BROKER;
    // const char *IP = TEST_BROKER;
    int port = 1883;
    // int port = 8883;
    const char *topic = TOPIC;
};

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

/* MQTT INIT*/

StaticJsonDocument<MQTT_MAX_PACKET_SIZE> json_fullData;
String jsonFullDataStr;
size_t jsonFullDataSize;

StaticJsonDocument<MQTT_MAX_PACKET_SIZE> json_sensorsData;
String jsonSensorDataStr;
size_t jsonSensorDataSize;

/* SCHEDULE SEQUENCE : READ_SENSOR -> ENCRYPT_DATA -> PREPARE_MQTT_PACKET -> SEND_DATA*/
struct Schedules
{
    unsigned long previousMillis = 0;
    unsigned long currentMillis = 0;
};

/* SENSORS */
DHT dht(DHTPIN, DHT_TYPE);
DFRobot_Heartrate heartrate(DIGITAL_MODE);

/* ENCRYPTION */

byte iv[16];
// = {
//     0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
//     0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};

byte data[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE - 32]; // we remove 16 because padding may add up to 16 and IV adds 16
byte preprocessed_data[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE - 16];
byte ciphertext[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE - 16];
size_t adjustedSize;                                      // data size + padding
byte encryptedSelf[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE]; // +16 since there is IV
size_t encryptedSelfSize;                                 // adjusted size + IV size
// String encodedData;                                       // base64encoded

byte nonEncryptedSelf[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE]; // +16 since there is IV
size_t nonEncryptedSelfSize;                                 // adjusted size + IV size
// unsigned long messageCount = 0;
byte mqttPacket[MQTT_MAX_PACKET_SIZE];
size_t mqttPacketSize;

size_t encryptedSelfBase64Size;
int base64EncodeLength;

BLE_Data bleData;
Broker_Config broker;
WIFI_Config wifi;
Schedules schedule;

WiFiClient wifiClient;
// WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
CBC<AES256> aes;

State state = INIT;

bool usePacketEncryption = USE_PACKET_ENCRYPTION;

bool bluetooth_init();
void wifi_init(const char *ssid, const char *password);
State check_if_end_of_bluetooth();

void handle_characteristic_changes();
void print_characteristic_values();

void setup()
{
    Serial.begin(115200);
    // while (!Serial)
    //     ;
    pinMode(ESP_LED_BUILTIN, OUTPUT);
    dht.begin();
}

void loop()
{
    switch (state)
    {
    case INIT:
        Serial.println("[STATE] Initialization");
        if (sizeof(bleData.senderIdString) && isUuidNotEmpty(bleData.senderToken, sizeof(bleData.senderToken)) && isUuidNotEmpty(bleData.contactId, sizeof(bleData.contactId)))
        {
            // then we can skip bluetooth step and directly start sending data
            state = SENDING_DATA;
        }
        else
        {
            isAdvertising = bluetooth_init();
            if (isAdvertising)
            {
                state = BLUETOOTH_KEY_CONFIG;
                Serial.println("[INFO] Listenning for BLE devices");
            }
        }

        // delay(3000);
        break;

    case BLUETOOTH_KEY_CONFIG:
        // led_blink(500, ESP_LED_BUILTIN);
        {
            BLEDevice central = BLE.central();
            if (central)
            {
                Serial.print("[INFO] BLE connected to central: ");
                Serial.println(central.address());

                while (central.connected())
                {
                    handle_characteristic_changes();
                    if (bleData.senderTokenReady && bleData.senderIdReady && bleData.encKeyReady && bleData.contactIdReady)
                    {
                        bleData.contactIdHexString = byteToHexString(bleData.contactId, sizeof(bleData.contactId));
                        bleData.senderTokenHexString = byteToHexString(bleData.senderToken, sizeof(bleData.senderToken));
                        bleData.encKeyHexString = byteToHexString(bleData.encKey, sizeof(bleData.encKey));
                        print_characteristic_values();

                        state = check_if_end_of_bluetooth();
                        if (state == SETUP_MQTT)
                        {
                            break;
                        }
                    }
                }
                Serial.print("[INFO] BLE disconnected from central: ");
                Serial.println(central.address());
                digitalWrite(ESP_LED_BUILTIN, LOW);
            }
            break;
        }

    case SETUP_MQTT:
        Serial.println("[STATE] SETUP MQTT");

        mqtt_packet_init(json_fullData, bleData.senderIdString, bleData.senderTokenHexString, bleData.contactIdHexString);

        wifi_init(wifi.ssid, wifi.pass);
        // TLS configuration  : client cert and client key to come
        // wifiClient.setCACert(CA_CERT);

        mqtt_broker_init(mqttClient, broker.IP, broker.port, ICURE_MQTT_ID, ICURE_MQTT_USER, ICURE_MQTT_PASSWORD);

        state = HANDLE_WAIT;
        break;

    case READ_SENSORS_DATA:
        Serial.println();
        Serial.println("[STATE] READING SENSORS");
        // messageCount += 1;
        mqttClient.loop();

        //{"content" : {"*":{"measureValue":{"value":"22","unit":"°C"}}} }

        json_sensorsData["content"]["*"]["measureValue"]["value"] = dht.readTemperature();
        json_sensorsData["content"]["*"]["measureValue"]["unit"] = "°C";
        json_sensorsData["content"]["*"]["measureValue"]["comment"] = "temperature";

        serializeJson(json_sensorsData, jsonSensorDataStr);
        jsonSensorDataSize = jsonSensorDataStr.length();

        jsonSensorDataStr.getBytes(data, jsonSensorDataSize + 1);
        Serial.println();
        // serializeJson(json_sensorsData, Serial);
        if (usePacketEncryption)
        {
            state = ENCRYPT_DATA;
        }
        else
        {
            state = PREPARE_MQTT_PACKET;
        }

        break;

    case ENCRYPT_DATA:
        Serial.println("[STATE] ENCRYPTING DATA");
        mqttClient.loop();
        adjustedSize = find_nearest_block_size(jsonSensorDataSize, AES_BLOCK_SIZE);
        encryptedSelfSize = adjustedSize + 16; // 16 padding + 16 IV bytes

        generate_IV(iv);

        apply_pkcs7(data, preprocessed_data, jsonSensorDataSize, adjustedSize);

        encrypt_CBC(&aes, bleData.encKey, adjustedSize, iv, preprocessed_data, ciphertext, AES_BLOCK_SIZE);

        state = PREPARE_MQTT_PACKET;
        break;
    case PREPARE_MQTT_PACKET:
        Serial.println("[STATE] PREPARE_MQTT_PACKET");

        mqttClient.loop();

        if (usePacketEncryption)
        {
            // to the full JSON,we add the encrypted part of the data : {"*":{"measureValue":{"value":"22","unit":"°C"}}}

            memcpy(encryptedSelf, iv, IV_SIZE);
            memcpy(encryptedSelf + IV_SIZE, ciphertext, adjustedSize);

            // arduino nano 33 IoT
            size_t encryptedSelfBase64Size = Base64.encodedLength(encryptedSelfSize);
            char encodedEncryptedSelf[encryptedSelfBase64Size];
            base64EncodeLength = Base64.encode(encodedEncryptedSelf, (char *)encryptedSelf, encryptedSelfSize);
            json_fullData["data"] = encodedEncryptedSelf;

            // ESP 32
            // String encodedData = base64::encode((char *)encryptedSelf);
            // // Serial.println((char *)encryptedSelf);
            // // Serial.println(encodedData);
            // json_fullData["data"] = encodedData;
        }
        else
        {
            memcpy(nonEncryptedSelf, data, jsonSensorDataSize);
            nonEncryptedSelfSize = jsonSensorDataSize;

            json_fullData["data"] = json_sensorsData;
        }

        serializeJson(json_fullData, Serial);
        serializeJson(json_fullData, jsonFullDataStr);

        jsonFullDataSize = jsonFullDataStr.length();
        jsonFullDataStr.getBytes(mqttPacket, jsonFullDataSize + 1);
        mqttPacketSize = jsonFullDataSize;

        state = SENDING_DATA;
        break;

    case SENDING_DATA:
        Serial.println();
        Serial.println("[STATE] SEND DATA TO BROKER");
        mqttClient.loop();

        // if (!mqttClient.connected())
        // {
        //     reconnect_to_broker(mqttClient);
        // }

        if (mqttPacketSize > MQTT_MAX_PACKET_SIZE)
        {
            Serial.println("[ERROR] Payload too big, too many bytes to send");
            state = ERROR;
            break;
        }
        else
        {
            if (mqttClient.connect(ICURE_MQTT_ID))
            {
                Serial.print("Connected, mqtt_mqttClient state: ");
                Serial.println(mqttClient.state());
                serializeJson(json_fullData, Serial);
                mqtt_publish(mqttClient, broker.topic, mqttPacket, mqttPacketSize);
            }
            else
            {
                Serial.println("Connected failed!  mqtt_mqttClient state:");
                Serial.print(mqttClient.state());
                Serial.println("WiFiClientSecure mqttClient state:");
                char lastError[100];
                // wifiClient.lastError(lastError, 100); // Get the last error for WiFiClientSecure
                // Serial.print(lastError);
                // free(lastError);
            }
        }

        state = HANDLE_WAIT;
        break;
    case HANDLE_WAIT:

        mqttClient.loop();

        schedule.currentMillis = millis();

        if (schedule.currentMillis - schedule.previousMillis >= INTERVAL_BETWEEN_MQTT_PACKETS)
        {
            state = READ_SENSORS_DATA;
            schedule.previousMillis = schedule.currentMillis;
        }
        break;

    case ERROR:
        Serial.println("[STATE] Error");
        delay(10000);
        break;
    }
}

bool bluetooth_init()
{
    while (!BLE.begin())
    {
        Serial.println("[ERROR] starting Bluetooth® Low Energy failed!");
        delay(3000);
    }
    String deviceAddress = BLE.address();
    Serial.print("[INFO] BLE Arduino Address: ");
    Serial.println(deviceAddress);

    BLE.setLocalName("iCure-device");
    BLE.setAdvertisedService(icureService);

    icureService.addCharacteristic(contactIdCharacteristic);
    icureService.addCharacteristic(senderIdCharacteristic);
    icureService.addCharacteristic(senderTokenCharacteristic);
    icureService.addCharacteristic(keyCharacteristic);
    icureService.addCharacteristic(statusCharacteristic);
    icureService.addCharacteristic(testCharacteristic);
    BLE.addService(icureService);

    statusCharacteristic.writeValue(0);

    bool advertising = BLE.advertise();
    return advertising;
}

void wifi_init(const char *ssid, const char *password)
{
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);

    // attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        // wait 1 second for re-trying
        delay(1000);
    }

    Serial.print("Connected to ");
    Serial.println(ssid);
}

State check_if_end_of_bluetooth()
{
    if (
        sizeof(bleData.senderIdString) && isUuidNotEmpty(bleData.senderToken, sizeof(bleData.senderToken)) && isUuidNotEmpty(bleData.encKey, sizeof(bleData.encKey)) && isUuidNotEmpty(bleData.contactId, sizeof(bleData.contactId)))
    {
        Serial.println("[INFO] BLE configuratin finished");

        statusCharacteristic.writeValue(1);
        delay(1000); // wait for notification to the android app

        if (BLE.connected())
        {
            BLE.disconnect();
            delay(100); // Allow time for disconnection
            BLE.stopAdvertise();
        }
        return SETUP_MQTT;
    }
    Serial.println("[INFO] Sender or encryption key not available");
    return BLUETOOTH_KEY_CONFIG;
}

void handle_characteristic_changes()
{
    if (testCharacteristic.written())
    {
        Serial.println();
        // toggle_led(&builtinLedOn, ESP_LED_BUILTIN);
    }
    if (senderTokenCharacteristic.written())
    {
        const byte *receivedToken = senderTokenCharacteristic.value();
        memcpy(bleData.senderToken, receivedToken, 16);
        bleData.senderTokenReady = true;
    }
    if (senderIdCharacteristic.written())
    {
        bleData.senderIdString = senderIdCharacteristic.value();
        bleData.senderIdReady = true;
    }
    if (contactIdCharacteristic.written())
    {
        const byte *receivedUuid = contactIdCharacteristic.value();
        memcpy(bleData.contactId, receivedUuid, 16);
        bleData.contactIdReady = true;
    }
    if (keyCharacteristic.written())
    {
        const byte *receivedKey = keyCharacteristic.value();
        memcpy(bleData.encKey, receivedKey, 32);
        bleData.encKeyReady = true;
    }
}
void print_characteristic_values()
{
    Serial.println("");

    Serial.print("contact Id");
    Serial.println(bleData.contactIdHexString);

    Serial.print("senderUuid ");
    Serial.println(bleData.senderIdString);

    Serial.print("token");
    Serial.println(bleData.senderTokenHexString);

    Serial.print("encryption key ");
    Serial.println(bleData.encKeyHexString);

    Serial.println("");
}
