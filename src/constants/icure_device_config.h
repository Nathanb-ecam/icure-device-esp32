#ifndef ICURE_DEVICE_CONFIG_H
#define ICURE_DEVICE_CONFIG_H

#include <device_global_config/secrets.h>

enum State
{
    INIT,
    BLUETOOTH_KEY_CONFIG,
    SETUP_MQTT,
    READ_AND_APPEND_SENSORS_DATA,
    COMPUTE_SENSOR_MEAN,
    PREPARE_SENSOR_DATA,
    ENCRYPT_DATA,
    PREPARE_MQTT_FULL_PACKET,
    RESET_SENSOR_READINGS,
    SEND_DATA,
    SCHEDULER,
    ERROR
};

bool builtinLedOn = false;

/* SCHEDULE SEQUENCE : READ_SENSOR -> ENCRYPT_DATA -> PREPARE_MQTT_PACKET -> SEND_DATA*/
struct Schedules
{
    // for scheduling the send of the messages
    unsigned long previousSendMillis = 0;
    unsigned long currentSendMillis = 0;
    // for scheduling the sensors reads
    unsigned long previousSensorMillis = 0;
    unsigned long currentSensorMillis = 0;
    // for triggered the come back to default sending frequency (after the behavior bas been changed for a too long period) 
    unsigned long previousBehaviorMillis = 0;
    unsigned long currentBehaviorMillis = 0;
};

struct WIFI_Config
{
    const char *ssid = SECRET_SSID; // your network SSID (name)
    const char *pass = SECRET_PASS;
};

#endif