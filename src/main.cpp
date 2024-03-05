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

#include <string.h>

/* MY UTILS */

#include "mqtt_utils.h"
#include "crypto_utils.h"
#include "led_utils.h"

/* CONSTANTS */
#include "secrets.h"
#include "constants/icure_device_config.h"
#include "constants/ble_constants.h"
#include "constants/crypto_constants.h"

#define INTERVAL_BETWEEN_MQTT_PACKETS 15000 // ms

// Humidity/temperature
#define DHT_TYPE DHT11
#define DHTPIN 17

#define heartratePin A1

#define ESP_LED_BUILTIN 2

StaticJsonDocument<MQTT_MAX_PACKET_SIZE> json_fullData;
String jsonFullDataStr;
size_t jsonFullDataSize;

StaticJsonDocument<MQTT_MAX_PACKET_SIZE> json_sensorsData;
String jsonSensorDataStr;
size_t jsonSensorDataSize;

/* SENSORS */
DHT dht(DHTPIN, DHT_TYPE);
DFRobot_Heartrate heartrate(DIGITAL_MODE);

/* FULL DATA TO BE SENT */
byte mqttPacket[MQTT_MAX_PACKET_SIZE];
size_t mqttPacketSize;

// USED ONLY IF UsePacketEncrypt is set to false
byte nonEncryptedSelf[MQTT_MAX_PACKET_SIZE - MAX_AUTH_SIZE]; // +16 since there is IV
size_t nonEncryptedSelfSize;                                 // adjusted size + IV size
// unsigned long messageCount = 0;

BLE_Data bleData;
Broker_Config broker;
WIFI_Config wifi;
Schedules schedule;

WiFiClient wifiClient;
// WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);

CBC<AES256> aes;
Encryption_Data encryptData;
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
            state = SEND_DATA;
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
                            BLE.stopAdvertise();
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

        jsonSensorDataStr.getBytes(encryptData.plaintext, jsonSensorDataSize + 1);
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
        encryptData.adjustedSize = find_nearest_block_size(jsonSensorDataSize, AES_BLOCK_SIZE);
        encryptData.encryptedSelfSize = encryptData.adjustedSize + 16; // 16 padding + 16 IV bytes

        generate_IV(encryptData.iv);

        apply_pkcs7(encryptData.plaintext, encryptData.plaintext_with_padding, jsonSensorDataSize, encryptData.adjustedSize);

        encrypt_CBC(&aes, bleData.encKey, encryptData.adjustedSize, encryptData.iv, encryptData.plaintext_with_padding, encryptData.ciphertext, AES_BLOCK_SIZE);

        state = PREPARE_MQTT_PACKET;
        break;
    case PREPARE_MQTT_PACKET:
        Serial.println("[STATE] PREPARE_MQTT_PACKET");

        mqttClient.loop();

        if (usePacketEncryption)
        {
            // to the full JSON,we add the encrypted part of the data : {"*":{"measureValue":{"value":"22","unit":"°C"}}}

            memcpy(encryptData.encryptedSelf, encryptData.iv, IV_SIZE);
            memcpy(encryptData.encryptedSelf + IV_SIZE, encryptData.ciphertext, encryptData.adjustedSize);

            encryptData.encryptedSelfBase64Size = Base64.encodedLength(encryptData.encryptedSelfSize);
            char encodedEncryptedSelf[encryptData.encryptedSelfBase64Size];
            encryptData.base64EncodeLength = Base64.encode(encodedEncryptedSelf, (char *)encryptData.encryptedSelf, encryptData.encryptedSelfSize);
            json_fullData["data"] = encodedEncryptedSelf;
        }
        else
        {
            memcpy(nonEncryptedSelf, encryptData.plaintext, jsonSensorDataSize);
            nonEncryptedSelfSize = jsonSensorDataSize;

            json_fullData["data"] = json_sensorsData;
        }

        serializeJson(json_fullData, Serial);
        serializeJson(json_fullData, jsonFullDataStr);

        jsonFullDataSize = jsonFullDataStr.length();
        jsonFullDataStr.getBytes(mqttPacket, jsonFullDataSize + 1);
        mqttPacketSize = jsonFullDataSize;

        state = SEND_DATA;
        break;

    case SEND_DATA:
        Serial.println();
        Serial.println("[STATE] SEND DATA TO BROKER");
        mqttClient.loop();

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
                reconnect_to_broker(mqttClient, ICURE_MQTT_ID, ICURE_MQTT_USER, ICURE_MQTT_PASSWORD);
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
        // Serial.println();
        toggle_led(&builtinLedOn, ESP_LED_BUILTIN);
    }
    if (senderTokenCharacteristic.written())
    {
        const byte *receivedToken = senderTokenCharacteristic.value();
        memcpy(bleData.senderToken, receivedToken, 16);
        bleData.senderTokenReady = true;
    }
    if (senderIdCharacteristic.written())
    {
        if (bleData.senderIdString.length() == 0)
        {
            bleData.senderIdString = senderIdCharacteristic.value();
        }
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
