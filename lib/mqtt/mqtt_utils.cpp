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



void reconnect_to_broker(MqttClient &client,const char *broker, int port, const char *mqttId, const char *mqttUser, const char *mqttPass, const char *topic)
{
    Serial.println("[DEBUG] reconnecting to broker ");
    while (!client.connect(broker, port)) {
        Serial.print(".");
        delay(1000);
    }
    client.subscribe(topic,1);
}

void callback(int messageSize)
{
    if (messageCallback)
    {
        messageCallback(messageSize);
    }
}

void setCallback(MessageCallback callback)
{
    messageCallback = callback;
}

bool mqtt_broker_init(MqttClient &client, const char *broker, int port, char *mqttId, const char *mqttUsername, const char *mqttPassword, const char *topic)
{
    client.setId(mqttId);
    client.setUsernamePassword(mqttUsername, mqttPassword);
    client.onMessage(callback);
    while (!client.connect(broker, port)) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println("[INFO] Connected to MQTT broker");
    Serial.print("[INFO] Using broker : ");
    Serial.print(broker);
    Serial.print(", port : ");
    Serial.println(port);
    client.subscribe(topic,1);
    Serial.print("[INFO] Subscribed to ");
    Serial.println(topic);
    return true;

}

void mqtt_publish(MqttClient &client, const char *topic, byte *packetBytes, size_t packetSize)
{
    if (client.beginMessage(topic, false, 1)) {
        client.write(packetBytes, packetSize);
        client.endMessage();
        Serial.println("[INFO] Message published successfully");
    } else {
        Serial.println("Failed to start the message");
    }
}

// void pubSubError(int8_t errCode){
//     switch (errorCode) {
//         case MQTT_CONNECTION_TIMEOUT:
//             Serial.println("Connection timeout");
//             break;
//         case MQTT_CONNECTION_LOST:
//             Serial.println("Connection lost");
//             break;
//         case MQTT_CONNECT_FAILED:
//             Serial.println("Connect failed");
//             break;
//         case MQTT_DISCONNECTED:
//             Serial.println("Disconnected");
//             break;
//         case MQTT_CONNECTED:
//             Serial.println("Connected");
//             break;
//         case MQTT_CONNECT_BAD_PROTOCOL:
//             Serial.println("Connect bad protocol");
//             break;
//         case MQTT_CONNECT_BAD_CLIENT_ID:
//             Serial.println("Connect bad Client-ID");
//             break;
//         case MQTT_CONNECT_UNAVAILABLE:
//             Serial.println("Connect unavailable");
//             break;
//         case MQTT_CONNECT_BAD_CREDENTIALS:
//             Serial.println("Connect bad credentials");
//             break;
//         case MQTT_CONNECT_UNAUTHORIZED:
//             Serial.println("Connect unauthorized");
//             break;
//         default:
//             Serial.println("Unknown error");
//     }
// }