#include <led_utils.h>

void led_blink(int delay_ms, uint8_t ledPin)
{
    // Serial.println("LED blink");
    digitalWrite(ledPin, HIGH);
    delay(delay_ms);
    digitalWrite(ledPin, LOW);
    delay(delay_ms);
    digitalWrite(ledPin, HIGH);
    delay(delay_ms);
    digitalWrite(ledPin, LOW);
    delay(delay_ms);
}

void toggle_led(bool *builtinLed, uint8_t ledPin)
{
    if (*builtinLed == false)
    {
        digitalWrite(ledPin, HIGH);
        *builtinLed = true;
    }
    else
    {
        digitalWrite(ledPin, LOW);
        *builtinLed = false;
    }
}