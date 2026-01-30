/**
 * @file MutualTLSMQTT.ino
 * @brief Example: MQTT with mutual TLS (client certificate authentication)
 * 
 * This example demonstrates connecting to an MQTT broker using mutual TLS,
 * where both the server and client present certificates for authentication.
 * This is the preferred method for production IoT deployments.
 * 
 * Mutual TLS provides:
 * - Server authentication (client verifies server certificate)
 * - Client authentication (server verifies client certificate)
 * - No need for username/password
 * - Strong cryptographic identity
 * 
 * Setup:
 * 1. Generate a client certificate and private key for your device
 * 2. Configure your MQTT broker to require client certificates
 * 3. Update mqtt_config.h with your certificates and broker settings
 * 
 * Compatible brokers:
 * - Mosquitto with require_certificate=true
 * - EMQX, HiveMQ, VerneMQ, etc.
 */

#include <AZ3166WiFi.h>
#include "AZ3166MQTTClient.h"
#include "mqtt_config.h"

AZ3166MQTTClient mqttClient;
int messageCount = 0;

void messageHandler(MQTT::MessageData& data)
{
    Serial.println("Received message:");
    
    // Extract and print topic
    int topicLen = data.topicName.lenstring.len;
    char* topicData = data.topicName.lenstring.data;
    Serial.print("  Topic: ");
    for (int i = 0; i < topicLen && i < 128; i++)
    {
        Serial.print(topicData[i]);
    }
    Serial.println();
    
    // Extract and print payload
    int payloadLen = data.message.payloadlen;
    char* payload = (char*)data.message.payload;
    Serial.print("  Payload (");
    Serial.print(payloadLen);
    Serial.print(" bytes): ");
    for (int i = 0; i < payloadLen && i < 256; i++)
    {
        Serial.print(payload[i]);
    }
    Serial.println();
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n========================================");
    Serial.println("  Mutual TLS MQTT Example");
    Serial.println("  Client Certificate Authentication");
    Serial.println("========================================\n");
    
    // Connect to WiFi
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30)
    {
        delay(500);
        Serial.print(".");
        wifiAttempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("\nFailed to connect to WiFi!");
        while (1) { delay(1000); }
    }
    
    Serial.println(" Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    // Connect with mutual TLS
    Serial.println("\nEstablishing mutual TLS connection...");
    Serial.print("  Broker: ");
    Serial.println(MQTT_HOST);
    Serial.print("  Port: ");
    Serial.println(MQTT_PORT);
    Serial.print("  Client ID: ");
    Serial.println(MQTT_CLIENT_ID);
    Serial.println("  Auth: Client Certificate (X.509)");
    
    int result = mqttClient.connectMutualTLS(
        MQTT_HOST,
        MQTT_PORT,
        CA_CERT,
        CLIENT_CERT,
        CLIENT_KEY,
        MQTT_CLIENT_ID,
        MQTT_CLIENT_ID  // Also pass client ID as username for Event Grid
    );
    
    if (result != 0)
    {
        Serial.print("\nConnection FAILED! Error code: ");
        Serial.println(result);
        Serial.println("\nMQTT Error codes:");
        Serial.println("  1 = Unacceptable protocol version");
        Serial.println("  2 = Identifier rejected");
        Serial.println("  3 = Server unavailable");
        Serial.println("  4 = Bad username/password");
        Serial.println("  5 = Not authorized");
        Serial.println("  Negative = Network/TLS error");
        while (1) { delay(1000); }
    }
    
    Serial.println("\n*** Connected with Mutual TLS! ***\n");
    
    // Subscribe to incoming messages
    Serial.print("Subscribing to: ");
    Serial.println(SUBSCRIBE_TOPIC);
    
    result = mqttClient.subscribe(SUBSCRIBE_TOPIC, MQTT::QOS1, messageHandler);
    if (result < 0)
    {
        Serial.print("Subscribe failed: ");
        Serial.println(result);
    }
    else
    {
        Serial.println("Subscribed successfully!");
    }
    
    Serial.println("\nReady. Publishing every 30 seconds...\n");
}

void loop()
{
    static unsigned long lastPublish = 0;
    unsigned long now = millis();
    
    // Publish every 30 seconds
    if (now - lastPublish >= 30000 || lastPublish == 0)
    {
        lastPublish = now;
        
        // Simulate sensor readings
        float temperature = 20.0 + (random(200) / 10.0);  // 20.0 - 40.0
        float humidity = 40.0 + (random(400) / 10.0);     // 40.0 - 80.0
        
        // Create JSON payload
        char payload[256];
        snprintf(payload, sizeof(payload),
            "{"
            "\"messageId\":%d,"
            "\"temperature\":%.2f,"
            "\"humidity\":%.2f,"
            "\"timestamp\":%lu"
            "}",
            messageCount++,
            temperature,
            humidity,
            now / 1000
        );
        
        Serial.print("[");
        Serial.print(messageCount);
        Serial.print("] Publishing: ");
        Serial.println(payload);
        
        int result = mqttClient.publish(PUBLISH_TOPIC, payload, 0, MQTT::QOS1);
        if (result != 0)
        {
            Serial.print("    Publish failed: ");
            Serial.println(result);
        }
        else
        {
            Serial.println("    Sent successfully");
        }
    }
    
    // Process incoming messages and keepalive
    if (!mqttClient.isConnected())
    {
        Serial.println("Connection lost! Reconnecting...");
        int result = mqttClient.connectMutualTLS(
            MQTT_HOST, MQTT_PORT, CA_CERT, CLIENT_CERT, CLIENT_KEY, MQTT_CLIENT_ID, MQTT_CLIENT_ID
        );
        if (result == 0)
        {
            Serial.println("Reconnected!");
            mqttClient.subscribe(SUBSCRIBE_TOPIC, MQTT::QOS1, messageHandler);
        }
    }
    
    mqttClient.loop(100);
}
