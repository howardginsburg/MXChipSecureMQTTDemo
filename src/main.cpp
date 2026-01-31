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
#include <time.h>

// Hardware objects
RGB_LED rgbLed;

AZ3166MQTTClient mqttClient;
int messageCount = 0;
static bool hasWifi = false;
static bool hasMqtt = false;

// LED color definitions
#define LED_OFF       0, 0, 0
#define LED_RED       255, 0, 0
#define LED_GREEN     0, 255, 0
#define LED_BLUE      0, 0, 255
#define LED_YELLOW    255, 255, 0
#define LED_MAGENTA   255, 0, 255

// Update OLED display
void updateDisplay(const char* line1, const char* line2 = NULL, const char* line3 = NULL)
{
    Screen.clean();
    Screen.print(0, line1);
    if (line2) Screen.print(1, line2);
    if (line3) Screen.print(2, line3);
}

// Update RGB LED based on connection status
void updateLEDs()
{
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

void setup()
{
    Serial.begin(115200);
    delay(1000);
    
    // Initialize OLED and LEDs
    Screen.init();
    Screen.clean();
    updateDisplay("Secure MQTT", "Initializing...");
    rgbLed.turnOff();
    
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
    
    Serial.println(" Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    
    updateDisplay("WiFi Connected", WiFi.localIP().get_address(), "Connecting MQTT...");
    
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
    
    Serial.println("\n*** Connected with Mutual TLS! ***\n");
    
    updateDisplay("MQTT Connected", MQTT_CLIENT_ID, "Ready!");
    Serial.println("\nReady. Publishing every 5 seconds...\n");
}

void loop()
{
    //Serial.println("Loop start");
    static unsigned long lastPublish = 0;
    unsigned long now = millis();
    
      
    // Check connection status
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
            Serial.println("Reconnected!");
            updateDisplay("Reconnected!", MQTT_CLIENT_ID, "Ready");
        }
        else
        {
            delay(2000);
        }
        return;
    }
    
    // Publish every 5 seconds
    if (now - lastPublish >= 5000 || lastPublish == 0)
    {
        lastPublish = now;
        
        // Simulate sensor readings
        float temperature = 20.0 + (random(200) / 10.0);  // 20.0 - 40.0
        float humidity = 40.0 + (random(400) / 10.0);     // 40.0 - 80.0
        
        // Get current time
        time_t now_time = time(NULL);
        struct tm* timeinfo = localtime(&now_time);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
        
        // Create JSON payload
        char payload[256];
        snprintf(payload, sizeof(payload),
            "{"
            "\"messageId\":%d,"
            "\"temperature\":%.2f,"
            "\"humidity\":%.2f,"
            "\"timestamp\":\"%s\""
            "}",
            messageCount++,
            temperature,
            humidity,
            timestamp
        );
        
        Serial.print("[");
        Serial.print(messageCount);
        Serial.print("] Publishing: ");
        Serial.println(payload);
        
        rgbLed.setColor(LED_MAGENTA);  // Flash magenta during publish
        
        int result = mqttClient.publish(PUBLISH_TOPIC, payload, strlen(payload), MQTT::QOS0);
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
            
            char tempStr[20], humStr[20], countStr[20];
            snprintf(tempStr, sizeof(tempStr), "Temp: %.1fC", temperature);
            snprintf(humStr, sizeof(humStr), "Hum:  %.1f%%", humidity);
            snprintf(countStr, sizeof(countStr), "Msg #%d sent", messageCount);
            updateDisplay("MQTT Active", tempStr, humStr);
        }
        
        updateLEDs();  // Restore normal state
    }
}
