#include <mqtt_utils.h>

static MessageCallback messageCallback = NULL;

// void mqtt_packet_init(JsonDocument &json_data, String senderId, char senderToken[25], char contactId[25])
void mqtt_packet_init(JsonDocument &json_data, String senderId, String senderToken, char contactId[25])
{
    json_data["uid"] = senderId;

    json_data["cid"] = contactId;

    json_data["token"] = senderToken;
}

void printPacket(byte *data, size_t dataSize)
{
    Serial.println("Packet");

    for (int i = 0; i < dataSize; i++)
    {
        Serial.print(data[i], HEX);
    }
    Serial.println();
}

void reconnect_to_broker(PubSubClient &client, const char *mqttId, const char *mqttUser, const char *mqttPass, const char *topic)
{
    while (!client.connected())
    {
        Serial.println("[INFO] Attempting MQTT reconnection...");
        if (client.connect(mqttId, mqttUser, mqttPass))
        {
            Serial.println("[INFO] Connected to MQTT broker");
            client.subscribe(topic);
            Serial.print("[INFO] Subscribed to ");
            Serial.println(topic);
        }
        else
        {
            Serial.print("[ERROR] Failed to connect to MQTT broker, rc=");
            Serial.print(client.state());
            Serial.println(" Trying again in 5 seconds...");
            delay(5000);
        }
    }
}

void callback(const char topic[], byte *payload, unsigned int length)
{
    if (messageCallback)
    {
        messageCallback(topic, payload, length);
    }
}

void setCallback(MessageCallback callback)
{
    messageCallback = callback;
}

bool mqtt_broker_init(PubSubClient &client, const char *broker, int port, char *mqttId, const char *mqttUsername, const char *mqttPassword, const char *topic)
{
    client.setServer(broker, port);
    client.setCallback(callback);
    if (client.connect(mqttId, mqttUsername, mqttPassword))
    {
        Serial.println("[INFO] Connected to MQTT broker");
        client.subscribe(topic);
        Serial.print("[INFO] Using broker : ");
        Serial.print(broker);
        Serial.print(", port : ");
        Serial.println(port);
        Serial.print("[INFO] Subscribed to ");
        Serial.println(topic);
        return true;
    }
    else
    {
        Serial.println("[ERROR] Failed to connect to MQTT broker");
        return false;
    }
}

void mqtt_publish(PubSubClient &client, const char topic[], byte *packetBytes, size_t packetSize)
{
    // printPacket(packetBytes, packetSize);
    // Serial.println("Publish");
    client.beginPublish(topic, packetSize, false);
    // Serial.print("Mqtt packet size:");
    // Serial.println(packetSize);
    for (int i = 0; i < packetSize; i++)
    {
        client.write(packetBytes[i]);
    }
    client.endPublish();
}

void pubSubError(int8_t errCode)
{
    if (errCode == MQTT_CONNECTION_TIMEOUT)
        Serial.println("Connection tiemout");
    else if (errCode == MQTT_CONNECTION_LOST)
        Serial.println("Connection lost");
    else if (errCode == MQTT_CONNECT_FAILED)
        Serial.println("Connect failed");
    else if (errCode == MQTT_DISCONNECTED)
        Serial.println("Disconnected");
    else if (errCode == MQTT_CONNECTED)
        Serial.println("Connected");
    else if (errCode == MQTT_CONNECT_BAD_PROTOCOL)
        Serial.println("Connect bad protocol");
    else if (errCode == MQTT_CONNECT_BAD_CLIENT_ID)
        Serial.println("Connect bad Client-ID");
    else if (errCode == MQTT_CONNECT_UNAVAILABLE)
        Serial.println("Connect unavailable");
    else if (errCode == MQTT_CONNECT_BAD_CREDENTIALS)
        Serial.println("Connect bad credentials");
    else if (errCode == MQTT_CONNECT_UNAUTHORIZED)
        Serial.println("Connect unauthorized");
}