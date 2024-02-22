#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H

#include <ArduinoJson.h>
#include <PubSubClient.h>

void mqtt_packet_init(JsonDocument &json_data, String senderIdString, String senderTokenString, String contactIdString);

void printPacket(byte *data, size_t dataSize);

void reconnect_to_broker(PubSubClient &client);

void mqtt_broker_init(PubSubClient &client, const char *broker, int port, char *mqttId, const char *mqttUsername, const char *mqttPassword);

void mqtt_publish(PubSubClient &client, const char topic[], byte *packetBytes, size_t packetSize);

#endif