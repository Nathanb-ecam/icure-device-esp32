#ifndef ICURE_DEVICE_CONFIG_H
#define ICURE_DEVICE_CONFIG_H

#include <secrets.h>

enum State
{
    INIT,
    BLUETOOTH_KEY_CONFIG,
    SETUP_MQTT,
    READ_SENSORS_DATA,
    ENCRYPT_DATA,
    PREPARE_MQTT_PACKET,
    HANDLE_WAIT,
    SEND_DATA,
    ERROR
};

bool builtinLedOn = false;

/* SCHEDULE SEQUENCE : READ_SENSOR -> ENCRYPT_DATA -> PREPARE_MQTT_PACKET -> SEND_DATA*/
struct Schedules
{
    unsigned long previousMillis = 0;
    unsigned long currentMillis = 0;
};

struct WIFI_Config
{
    const char *ssid = SECRET_SSID; // your network SSID (name)
    const char *pass = SECRET_PASS;
};

#endif