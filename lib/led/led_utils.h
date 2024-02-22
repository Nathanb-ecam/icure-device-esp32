#ifndef LED_UTILS_H
#define LED_UTILS_H

#include <Arduino.h>

void led_blink(int delay_ms, uint8_t ledPin);

void toggle_led(bool *builtinLed, uint8_t ledPin);

#endif