# iCure IOT device

### To upload the code to the device (esp32)

1. Add a device_global_config folder under the src directory
2. Create a `secrets.h` file within that folder :

```h
#ifndef SECRETS_H
#define SECRETS_H

#define SECRET_SSID "WiFiName"
#define SECRET_PASS "WiFiPassword"

// should always be set to true
#define USE_PACKET_ENCRYPTION true

// define the uuid's of the bluetooth characteristics, they must match the ones of the android app :
// example with random uuid
#define BLE_SVC_UUID "950e0281-0e73-4477-9c41-e6ff53dc1c1a"
#define BLE_SENDER_ID_UUID ""
#define BLE_CONTACT_ID ""
#define BLE_TOPIC_UUID ""
#define BLE_SENDER_TOKEN_UUID ""
#define BLE_TEST_UUID ""
#define BLE_KEY_UUID ""
#define BLE_STATUS_UUID ""


#endif
```

3. Create a `broker_config.h` file still within the `device_global_config` folder :
   broker_config.h file template :

```h
#ifndef BROKER_CONFIG_H
#define BROKER_CONFIG_H

#define ICURE_MQTT_ID "mqttClientId" // can be anything
#define ICURE_MQTT_USER "mqttUsername"
#define ICURE_MQTT_PASSWORD "mqttPassword"

#define INCOMING_COMMANDS_TOPIC "topicToListenTo"

#define BROKER_IP "broker.domain.name"
#define BROKER_PORT 8883

static const char CA_CERT[] PROGMEM = R"EOF(
-----BEGIN CERTIFICATE-----

-----END CERTIFICATE-----
)EOF";


// IF USING A BROKER WITH TWO WAY SSL
// static const char CLIENT_KEY[] PROGMEM = R"EOF(
// -----BEGIN CERTIFICATE-----

// -----END CERTIFICATE-----
// )EOF";

// static const char CLIENT_CERT[] PROGMEM = R"EOF(
// -----BEGIN CERTIFICATE-----

// -----END CERTIFICATE-----

// )EOF";

#endif
```

### Principal steps :

1. The device receives data (username, password, contactId and the contacts symmetric key) by bluetooth from the android app (Android_device_configurator).
2. The device reads data from his sensors (currently a temperature)
3. Encrypts the sensors data (using AES256-CBC-PKCS7) to produce the encryptedSelf field of a contact.
4. Sends data to the broker through a mqtt connection.
5. Listens for incoming "command messages" to adapt the sending behavior of the microcontroller (currenlty sets the frequency)

### Problems :

- verification of the received bluetooth values (currently only checks that some data is received but not ensure its correct format)
