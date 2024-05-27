#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H

#include <ArduinoJson.h>
#include <ArduinoMqttClient.h>

typedef void (*MessageCallback)(int);

void setCallback(MessageCallback callback);

// void mqtt_packet_init(JsonDocument &json_data, String senderId, char senderToken[25], char contactId[25]);
void mqtt_packet_init(JsonDocument &json_data, String senderId, String senderToken, char contactId[25]);

void printPacket(byte *data, size_t dataSize);

void reconnect_to_broker(MqttClient &client, const char *broker, int port,const char *mqttId, const char *mqttUser, const char *mqttPass, const char *topic);

void callback(int messageSize);

bool mqtt_broker_init(MqttClient &client, const char *broker, int port, char *mqttId, const char *mqttUsername, const char *mqttPassword, const char *topic);

void mqtt_publish(MqttClient &client, const char topic[], byte *packetBytes, size_t packetSize);

// void pubSubError(int8_t errCode);

#endif