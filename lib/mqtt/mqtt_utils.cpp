#include <mqtt_utils.h>

// void mqtt_packet_init(JsonDocument &json_data, String senderId, byte senderToken[16], byte contactId[16])
void mqtt_packet_init(JsonDocument &json_data, String senderId, char senderToken[25], char contactId[25])
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

void reconnect_to_broker(PubSubClient &client, const char *mqttId, const char *mqttUser, const char *mqttPass)
{
    while (!client.connected())
    {
        Serial.println("[INFO] Attempting MQTT reconnection...");
        if (client.connect(mqttId, mqttUser, mqttPass))
        {
            Serial.println("[INFO] Connected to MQTT broker");
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

void mqtt_broker_init(PubSubClient &client, const char *broker, int port, char *mqttId, const char *mqttUsername, const char *mqttPassword)
{
    client.setServer(broker, port);
    client.connect(mqttId, mqttUsername, mqttPassword);
    Serial.print("[INFO] Using broker : ");
    Serial.print(broker);
    Serial.print(", port : ");
    Serial.println(port);
}

void mqtt_publish(PubSubClient &client, const char topic[], byte *packetBytes, size_t packetSize)
{

    // Serial.println("Publish");
    client.beginPublish(topic, packetSize, false);
    // Serial.println(packetSize);
    for (int i = 0; i < packetSize; i++)
    {
        client.write(packetBytes[i]);
    }
    // for (int i = 0; i < encryptedDataSize; i++)
    // {
    //     client.write(encryptedData[i]);
    // }
    client.endPublish();

    // DEBUG
    // printPacket(packetBytes, packetSize);
}
