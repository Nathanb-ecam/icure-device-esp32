#include <Arduino.h>

#include <Base64.h>
#include <DHT.h>
#include <DFRobot_Heartrate.h>

#include <CryptoAES_CBC.h>
#include <AES.h>
#include <CBC.h>

// #include <PubSubClient.h>
#include <ArduinoMqttClient.h>
#include <ArduinoJson.h>

#include <WiFiClientSecure.h>
#include <ArduinoBLE.h>

#include <string.h>

/* MY UTILS */

#include "mqtt_utils.h"
#include "crypto_utils.h"
#include "led_utils.h"

/* CONSTANTS */
#include "device_global_config/secrets.h"
#include "constants/icure_device_config.h"
#include "constants/ble_constants.h"
#include "constants/crypto_constants.h"
#include "constants/mqtt_constants.h"

/* SSL MQTT BROKER CONFIG */
#include "device_global_config/broker_config.h"

// #define INTERVAL_BETWEEN_MQTT_PACKETS 10000 // ms

// Humidity/temperature
#define DHT_TYPE DHT11
#define DHTPIN 17
#define heartratePin A1
#define ESP_LED_BUILTIN 2

struct Broker_Config
{
    const char *IP = BROKER_IP;
    int port = BROKER_PORT;
    const char commandTopic[15] = INCOMING_COMMANDS_TOPIC; // to listen for kraken asking to limit message frequency
};

/*  JSON FOR MESSAGES SENT TO THE BROKER */
StaticJsonDocument<MQTT_MAX_OUTGOING_PACKET_SIZE> json_fullData;
String jsonFullDataStr;
size_t jsonFullDataSize;

StaticJsonDocument<MQTT_MAX_OUTGOING_PACKET_SIZE> json_sensorsData;
String jsonSensorDataStr;
size_t jsonSensorDataSize;

/* JSON FOR INCOMMING MESSAGES : BIDIRECTIONNAL COMUNICATION TO RECEIVE COMMANDS FROM BACKEND */

StaticJsonDocument<MQTT_MAX_INCOMING_PACKET_SIZE> incommingMessageDoc;

/* SENSORS */
DHT dht(DHTPIN, DHT_TYPE);
DFRobot_Heartrate heartrate(DIGITAL_MODE);
String temperatureString;
float roundedTemperature;

/* FULL DATA TO BE SENT */
byte mqttPacket[MQTT_MAX_OUTGOING_PACKET_SIZE];
size_t mqttPacketSize;

// USED ONLY IF UsePacketEncrypt from "device_globam_config/secrets.h" is set to false
byte nonEncryptedSelf[MQTT_MAX_OUTGOING_PACKET_SIZE - MAX_AUTH_SIZE]; // +16 since there is IV
size_t nonEncryptedSelfSize;                                          // adjusted size + IV size
// unsigned long messageCount = 0;

BLE_Data bleData;
Broker_Config broker;
WIFI_Config wifi;
Schedules schedule;

unsigned long INTERVAL_BETWEEN_MQTT_PACKETS = 10000; // ms

// WiFiClient wifiClient;
WiFiClientSecure wifiClient;
// PubSubClient mqttClient(wifiClient);
MqttClient mqttClient(wifiClient);

CBC<AES256> aes;
Encryption_Data encryptData;
State state = INIT;
// State state = SETUP_MQTT;



byte incomingPayloadBuffer[MQTT_MAX_INCOMING_PACKET_SIZE]; // Buffer to store the payload bytes
size_t incomingPayloadLength = 0; 

bool successfullyInitialized = false;
bool usePacketEncryption = USE_PACKET_ENCRYPTION;

bool bluetooth_init();
void onMessageReceived(int messageSize);
void wifi_init(const char *ssid, const char *password);
State check_if_end_of_bluetooth();

bool patient_informations_ready();

void handle_characteristic_changes();

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
        bleData.patientDataReady = patient_informations_ready();
        if (bleData.patientDataReady)
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
        led_blink(500, ESP_LED_BUILTIN);
        {
            BLEDevice central = BLE.central();
            if (central)
            {
                Serial.print("[INFO] BLE connected to central: ");
                Serial.println(central.address());

                while (central.connected())
                {
                    handle_characteristic_changes();
                    if (bleData.senderTokenReady && bleData.senderIdReady && bleData.encKeyReady && bleData.contactIdReady && bleData.upstreamTopicReady)
                    {

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
        mqtt_packet_init(json_fullData, bleData.senderIdString, bleData.senderTokenString, bleData.contactIdBase64Encoded);

        wifi_init(wifi.ssid, wifi.pass);

        // TLS configuration  : client cert and client key to come
        wifiClient.setCACert(CA_CERT);
        // wifiClient.setCertificate(CLIENT_CERT);
        // wifiClient.setPrivateKey(CLIENT_KEY);

        setCallback(onMessageReceived);
        successfullyInitialized = mqtt_broker_init(mqttClient, BROKER_IP, BROKER_PORT, ICURE_MQTT_ID, ICURE_MQTT_USER, ICURE_MQTT_PASSWORD, INCOMING_COMMANDS_TOPIC);
        if (!successfullyInitialized)
        {
            state = ERROR;
        }

        

        state = HANDLE_WAIT;
        break;

    case READ_SENSORS_DATA:
        Serial.println("------------------------------------------------------------------------");
        Serial.println("[STATE] READING SENSORS");
        // messageCount += 1;
        mqttClient.poll();

        //{"content" : {"*":{"measureValue":{"value":"22","unit":"°C"}}}}

        temperatureString = String(dht.readTemperature(), 1);
        roundedTemperature = temperatureString.toFloat();
        json_sensorsData["content"]["*"]["measureValue"]["value"] = temperatureString;
        json_sensorsData["content"]["*"]["measureValue"]["unit"] = "°C";
        json_sensorsData["content"]["*"]["measureValue"]["comment"] = "temperature";

        serializeJson(json_sensorsData, jsonSensorDataStr);
        jsonSensorDataSize = jsonSensorDataStr.length();

        jsonSensorDataStr.getBytes(encryptData.plaintext, jsonSensorDataSize + 1);

        
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
        mqttClient.poll();
        encryptData.adjustedSize = find_nearest_block_size(jsonSensorDataSize, AES_BLOCK_SIZE);
        encryptData.encryptedSelfSize = encryptData.adjustedSize + 16; // 16 padding + 16 IV bytes

        generate_IV(encryptData.iv);

        apply_pkcs7(encryptData.plaintext, encryptData.plaintext_with_padding, jsonSensorDataSize, encryptData.adjustedSize);
        
        encrypt_CBC(&aes, bleData.encKey, encryptData.adjustedSize, encryptData.iv, encryptData.plaintext_with_padding, encryptData.ciphertext, AES_BLOCK_SIZE);

        state = PREPARE_MQTT_PACKET;
        break;
    case PREPARE_MQTT_PACKET:
        Serial.println("[STATE] PREPARE_MQTT_PACKET");

        mqttClient.poll();

        if (usePacketEncryption)
        {
            // to the full JSON,we add the encrypted part of the data : {"content" : {"*":{"measureValue":{"value":"22","unit":"°C"}}} }

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

        // serializeJson(json_fullData, Serial);
        serializeJson(json_fullData, jsonFullDataStr);



        jsonFullDataSize = jsonFullDataStr.length();
        jsonFullDataStr.getBytes(mqttPacket, jsonFullDataSize + 1);
        mqttPacketSize = jsonFullDataSize;

        state = SEND_DATA;
        break;

    case SEND_DATA:
        Serial.println("[STATE] SENDING DATA TO BROKER");
        mqttClient.poll();

        if (mqttPacketSize > MQTT_MAX_OUTGOING_PACKET_SIZE)
        {
            Serial.println("[ERROR] Payload too big, too many bytes to send");
            state = ERROR;
            break;
        }
        else
        {
            if (!mqttClient.connected())
            {
                reconnect_to_broker(mqttClient, broker.IP, broker.port,ICURE_MQTT_ID, ICURE_MQTT_USER, ICURE_MQTT_PASSWORD, INCOMING_COMMANDS_TOPIC);
            }
            mqtt_publish(mqttClient, bleData.upstreamTopic.c_str(), mqttPacket, mqttPacketSize);
            Serial.print("Publishing topic: ");
            Serial.println(bleData.upstreamTopic.c_str());
        }

        state = HANDLE_WAIT;
        break;
    case HANDLE_WAIT:

        mqttClient.poll();

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
    icureService.addCharacteristic(topicCharacteristic);
    icureService.addCharacteristic(statusCharacteristic);
    icureService.addCharacteristic(testCharacteristic);
    BLE.addService(icureService);

    statusCharacteristic.writeValue(0);

    bool advertising = BLE.advertise();
    return advertising;
}

void wifi_init(const char *ssid, const char *password)
{
    // Serial.print("Attempting to connect to SSID: ");
    // Serial.println(ssid);
    WiFi.begin(ssid, password);
    Serial.print("[INFO]");
    // attempt to connect to Wifi network:
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        // wait 1 second for re-trying
        delay(1000);
    }
    Serial.print("connected to ");
    Serial.println(ssid);
}

bool patient_informations_ready()
{
    if (
        sizeof(bleData.senderIdString) &&
        sizeof(bleData.senderTokenString) &&
        sizeof(bleData.upstreamTopic) &&
        bleData.contactIdBase64Encoded[0] != '\0' &&
        !isArrayEmpty(bleData.encKey, sizeof(bleData.encKey)))
    {
        return true;
    }
    return false;
}

State check_if_end_of_bluetooth()
{
    bleData.patientDataReady = patient_informations_ready();
    if (bleData.patientDataReady)
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
    if (senderIdCharacteristic.written())
    {
        // if (bleData.senderIdString.length() == 0)
        // {
        bleData.senderIdString = senderIdCharacteristic.value();
        Serial.println(bleData.senderIdString);
        // }
        bleData.senderIdReady = true;
    }
    if (senderTokenCharacteristic.written())
    {
        // if(bleData.senderTokenString.length() == 0){
        //     bleData.senderTokenString = senderTokenCharacteristic.value();
        // }

        bleData.senderTokenString = senderTokenCharacteristic.value();
        Serial.println(bleData.senderTokenString);
        bleData.senderTokenReady = true;
    }
    if (topicCharacteristic.written())
    {
        bleData.upstreamTopic = topicCharacteristic.value();
        bleData.upstreamTopicReady = true;
        Serial.print("topic ");
        Serial.println(bleData.upstreamTopic);
    }
    if (contactIdCharacteristic.written())
    {
        const byte *receivedUuid = contactIdCharacteristic.value();
        Base64.encode(bleData.contactIdBase64Encoded, (char *)receivedUuid, 16);
        bleData.contactIdBase64Encoded[24] = '\0';
        Serial.println(bleData.contactIdBase64Encoded);
        bleData.contactIdReady = true;
    }
    if (keyCharacteristic.written())
    {
        const byte *receivedKey = keyCharacteristic.value();
        memcpy(bleData.encKey, receivedKey, 32);
        bleData.encKeyHexString = byteToHexString(bleData.encKey, sizeof(bleData.encKey));
        Serial.println(bleData.encKeyHexString);
        bleData.encKeyReady = true;
    }
}

void onMessageReceived(int messageSize)
{
    incomingPayloadLength = 0;
    while (mqttClient.available() && incomingPayloadLength < MQTT_MAX_INCOMING_PACKET_SIZE) {
        byte nextByte = mqttClient.read();
        incomingPayloadBuffer[incomingPayloadLength++] = nextByte;
    }

    
    if (incomingPayloadLength >= MQTT_MAX_INCOMING_PACKET_SIZE) {
        Serial.println("Payload exceeds buffer size, some bytes may be truncated");
    }

    
    Serial.print("Payload: ");
    for (size_t i = 0; i < incomingPayloadLength; i++) {
        Serial.print(incomingPayloadBuffer[i]);
        // Serial.print(" ");
    }


    char json[messageSize + 1];
    memcpy(json, incomingPayloadBuffer, messageSize);
    json[messageSize] = '\0';

    DeserializationError error = deserializeJson(incommingMessageDoc, json);

    if (error)
    {
        Serial.print(F("deserializeJson() failed on incoming message : "));
        Serial.println(error.f_str());
        return;
    }

    if (incommingMessageDoc.containsKey("command"))
    {
        const char *command = incommingMessageDoc["command"];

        if (strcmp(command, "reduce") == 0)
        {
            Serial.println("Handling reduce command");
        }
        else if (strcmp(command, "increase") == 0)
        {
            Serial.println("Handling increase command");
        }
        else
        {
            Serial.println("Incoming command not handled");
        }
    }
    else
    {
        Serial.println("Uncompatible incoming message content");
    }

    Serial.println();
}
