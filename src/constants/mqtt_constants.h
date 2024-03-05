#ifndef MQTT_CONSTANTS_H
#define MQTT_CONSTANTS_H

#include <secrets.h>

#define MQTT_MAX_PACKET_SIZE 1024
#define MAX_AUTH_SIZE 64

#define ICURE_MQTT_ID "ESP32-iCure"
#define ICURE_MQTT_USER "iCureIoTUser"
#define ICURE_MQTT_PASSWORD "iCureIoTPassword"
#define TOPIC "icure_nano_topic/sub_topic"

struct Broker_Config
{
    const char *IP = LOCAL_BROKER;
    // const char *IP = TEST_BROKER;
    int port = 1883;
    // int port = 8883;
    const char *topic = TOPIC;
};

#endif