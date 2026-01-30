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
#include "OledDisplay.h"
#include "RGB_LED.h"
#include "mqtt_config.h"

// Hardware objects
RGB_LED rgbLed;
static bool hasWifi = false;
static bool hasMqtt = false;

AZ3166MQTTClient mqttClient;
int messageCount = 0;

// LED color definitions
#define LED_OFF       0, 0, 0
#define LED_RED       255, 0, 0
#define LED_GREEN     0, 255, 0
#define LED_BLUE      0, 0, 255
#define LED_YELLOW    255, 255, 0
#define LED_CYAN      0, 255, 255
#define LED_MAGENTA   255, 0, 255
#define LED_WHITE     255, 255, 255

// Display status on OLED
void updateDisplay(const char* line1, const char* line2 = NULL, 
                   const char* line3 = NULL, const char* line4 = NULL)
{
    Screen.clean();
    Screen.print(0, line1);
    if (line2) Screen.print(1, line2);
    if (line3) Screen.print(2, line3);
    if (line4) Screen.print(3, line4);
}

// Show connection status on LEDs
void updateLEDs()
{
    // WiFi LED (directly controlled)
    digitalWrite(LED_WIFI, hasWifi ? HIGH : LOW);
    
    // Azure LED (MQTT connected)
    digitalWrite(LED_AZURE, hasMqtt ? HIGH : LOW);
    
    // RGB LED shows combined status
    if (!hasWifi)
    {
        rgbLed.setColor(LED_RED);  // No WiFi - Red
    }
    else if (!hasMqtt)
    {
        rgbLed.setColor(LED_YELLOW);  // WiFi OK, no MQTT - Yellow
    }
    else
    {
        rgbLed.setColor(LED_GREEN);  // All connected - Green
    }
}

// Blink User LED for activity indication
void blinkUserLed(int times = 1)
{
    for (int i = 0; i < times; i++)
    {
        digitalWrite(LED_USER, HIGH);
        delay(50);
        digitalWrite(LED_USER, LOW);
        if (i < times - 1) delay(50);
    }
}

void messageHandler(MQTT::MessageData& data)
{
    // Blink LED on message receive
    blinkUserLed(2);
    rgbLed.setColor(LED_CYAN);  // Flash cyan for received message
    
    Serial.println("Received message:");
    
    // Extract topic
    int topicLen = data.topicName.lenstring.len;
    char* topicData = data.topicName.lenstring.data;
    
    // Create topic string for display
    char topicStr[32];
    int copyLen = (topicLen < 31) ? topicLen : 31;
    strncpy(topicStr, topicData, copyLen);
    topicStr[copyLen] = '\0';
    
    Serial.print("  Topic: ");
    Serial.println(topicStr);
    
    // Extract payload
    int payloadLen = data.message.payloadlen;
    char* payload = (char*)data.message.payload;
    
    // Create payload string for display (truncate if needed)
    char payloadStr[64];
    int payloadCopyLen = (payloadLen < 63) ? payloadLen : 63;
    strncpy(payloadStr, payload, payloadCopyLen);
    payloadStr[payloadCopyLen] = '\0';
    
    Serial.print("  Payload: ");
    Serial.println(payloadStr);
    
    // Show on OLED
    updateDisplay("Msg Received:", topicStr, payloadStr);
    
    delay(100);
    updateLEDs();  // Restore normal LED state
}

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    // Initialize LEDs
    pinMode(LED_WIFI, OUTPUT);
    pinMode(LED_AZURE, OUTPUT);
    pinMode(LED_USER, OUTPUT);
    rgbLed.turnOff();
    
    // Initialize OLED
    Screen.init();
    Screen.clean();
    updateDisplay("Secure MQTT", "Initializing...");
    rgbLed.setColor(LED_BLUE);  // Initializing - Blue
    
    Serial.println("\n========================================");
    Serial.println("  Mutual TLS MQTT Example");
    Serial.println("  Client Certificate Authentication");
    Serial.println("========================================\n");
    
    // Connect to WiFi
    updateDisplay("Secure MQTT", "Connecting WiFi", WIFI_SSID);
    Serial.print("Connecting to WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    int wifiAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && wifiAttempts < 30)
    {
        delay(500);
        Serial.print(".");
        // Blink RGB LED while connecting
        rgbLed.setColor((wifiAttempts % 2) ? LED_BLUE : LED_OFF);
        wifiAttempts++;
    }
    
    if (WiFi.status() != WL_CONNECTED)
    {
        hasWifi = false;
        updateLEDs();
        updateDisplay("WiFi FAILED!", "Check config", WIFI_SSID);
        Serial.println("\nFailed to connect to WiFi!");
        while (1) { delay(1000); }
    }
    
    hasWifi = true;
    updateLEDs();
    
    char ipStr[20];
    IPAddress ip = WiFi.localIP();
    snprintf(ipStr, sizeof(ipStr), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    
    Serial.println(" Connected!");
    Serial.print("IP: ");
    Serial.println(ipStr);
    
    updateDisplay("WiFi Connected", ipStr, "Connecting MQTT...");
    
    // Connect with mutual TLS
    Serial.println("\nEstablishing mutual TLS connection...");
    Serial.print("  Broker: ");
    Serial.println(MQTT_HOST);
    Serial.print("  Port: ");
    Serial.println(MQTT_PORT);
    Serial.print("  Client ID: ");
    Serial.println(MQTT_CLIENT_ID);
    Serial.println("  Auth: Client Certificate (X.509)");
    
    rgbLed.setColor(LED_YELLOW);  // Connecting MQTT - Yellow
    
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
        hasMqtt = false;
        updateLEDs();
        
        char errStr[20];
        snprintf(errStr, sizeof(errStr), "Error: %d", result);
        updateDisplay("MQTT FAILED!", errStr, MQTT_HOST);
        
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
    
    hasMqtt = true;
    updateLEDs();
    blinkUserLed(3);  // Success indication
    
    Serial.println("\n*** Connected with Mutual TLS! ***\n");
    
    // Subscribe to incoming messages
    Serial.print("Subscribing to: ");
    Serial.println(SUBSCRIBE_TOPIC);
    
    result = mqttClient.subscribe(SUBSCRIBE_TOPIC, MQTT::QOS1, messageHandler);
    if (result < 0)
    {
        Serial.print("Subscribe failed: ");
        Serial.println(result);
        updateDisplay("MQTT Connected", "Subscribe FAILED", SUBSCRIBE_TOPIC);
    }
    else
    {
        Serial.println("Subscribed successfully!");
        updateDisplay("MQTT Connected", MQTT_CLIENT_ID, "Ready!");
    }
    
    Serial.println("\nReady. Publishing every 30 seconds...\n");
}

void loop()
{
    static unsigned long lastPublish = 0;
    static unsigned long lastDisplayUpdate = 0;
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
        
        // Flash LED during publish
        rgbLed.setColor(LED_MAGENTA);
        blinkUserLed(1);
        
        int result = mqttClient.publish(PUBLISH_TOPIC, payload, 0, MQTT::QOS1);
        if (result != 0)
        {
            Serial.print("    Publish failed: ");
            Serial.println(result);
            rgbLed.setColor(LED_RED);
            
            char errStr[32];
            snprintf(errStr, sizeof(errStr), "Pub failed: %d", result);
            updateDisplay("Publish Error", errStr);
        }
        else
        {
            Serial.println("    Sent successfully");
            
            // Update display with last published data
            char tempStr[20], humStr[20], countStr[20];
            snprintf(tempStr, sizeof(tempStr), "Temp: %.1fC", temperature);
            snprintf(humStr, sizeof(humStr), "Hum:  %.1f%%", humidity);
            snprintf(countStr, sizeof(countStr), "Msg #%d sent", messageCount);
            updateDisplay("MQTT Active", tempStr, humStr, countStr);
        }
        
        updateLEDs();  // Restore normal state
    }
    
    // Process incoming messages and keepalive
    if (!mqttClient.isConnected())
    {
        hasMqtt = false;
        updateLEDs();
        updateDisplay("Disconnected!", "Reconnecting...", MQTT_HOST);
        
        Serial.println("Connection lost! Reconnecting...");
        int result = mqttClient.connectMutualTLS(
            MQTT_HOST, MQTT_PORT, CA_CERT, CLIENT_CERT, CLIENT_KEY, MQTT_CLIENT_ID, MQTT_CLIENT_ID
        );
        if (result == 0)
        {
            hasMqtt = true;
            updateLEDs();
            blinkUserLed(2);
            Serial.println("Reconnected!");
            mqttClient.subscribe(SUBSCRIBE_TOPIC, MQTT::QOS1, messageHandler);
            updateDisplay("Reconnected!", MQTT_CLIENT_ID, "Ready");
        }
    }
    
    mqttClient.loop(100);
}
