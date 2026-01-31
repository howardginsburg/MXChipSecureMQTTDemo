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
    // Set discrete LEDs
    digitalWrite(LED_AZURE, hasMqtt ? HIGH : LOW);      // Azure LED on when MQTT connected
    digitalWrite(LED_USER, (hasWifi && hasMqtt) ? HIGH : LOW);  // User LED on when all good (WiFi + MQTT)
    
    // Set RGB LED status indicator
    if (!hasWifi)
    {
        rgbLed.setColor(LED_RED);  // No WiFi - Red
    }
    else if (!hasMqtt)
    {
        rgbLed.setColor(LED_YELLOW);  // WiFi OK, no MQTT - Yellow
    }
    
}

// Flash main RGB LED blue
void flashBlue()
{
    rgbLed.setColor(LED_BLUE);
    delay(500);
    rgbLed.turnOff();  // Restore normal state
}

// Connect to MQTT broker
int connectMQTT()
{
    Serial.println("\nEstablishing mutual TLS connection...");
    Serial.print("  Broker: ");
    Serial.println(MQTT_HOST);
    Serial.print("  Port: ");
    Serial.println(MQTT_PORT);
    Serial.println("  Auth: Client Certificate (X.509)");
    
    rgbLed.setColor(LED_YELLOW);
    
    int result = mqttClient.connectMutualTLS(
        MQTT_HOST, MQTT_PORT, CA_CERT, CLIENT_CERT, CLIENT_KEY, MQTT_CLIENT_ID, MQTT_CLIENT_ID
    );
    
    if (result != 0)
    {
        char errStr[20];
        snprintf(errStr, sizeof(errStr), "Error: %d", result);
        updateDisplay("MQTT FAILED!", errStr, MQTT_HOST);
        Serial.print("Connection FAILED! Code: ");
        Serial.println(result);
    }
    
    return result;
}

// Build JSON payload with sensor data
void buildPayload(char* buffer, size_t size, float temp, float humidity)
{
    time_t now_time = time(NULL);
    struct tm* timeinfo = localtime(&now_time);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    snprintf(buffer, size,
        "{"
        "\"messageId\":%d,"
        "\"temperature\":%.2f,"
        "\"humidity\":%.2f,"
        "\"timestamp\":\"%s\""
        "}",
        messageCount++,
        temp,
        humidity,
        timestamp
    );
}

// Publish telemetry data
void publishTelemetry()
{
    float temperature = 20.0 + (random(200) / 10.0);
    float humidity = 40.0 + (random(400) / 10.0);
    
    char payload[256];
    buildPayload(payload, sizeof(payload), temperature, humidity);
    
    Serial.print("[");
    Serial.print(messageCount);
    Serial.print("] Publishing: ");
    Serial.println(payload);
    
    //rgbLed.setColor(LED_MAGENTA);
    
    int result = mqttClient.publish(PUBLISH_TOPIC, payload, strlen(payload), MQTT::QOS0);
    
    if (result != 0)
    {
        Serial.print("    Publish failed: ");
        Serial.println(result);
        rgbLed.setColor(LED_RED);
        updateDisplay("Publish Error", "Failed");
    }
    else
    {
        Serial.println("    Sent successfully");
        
        char tempStr[20], humStr[20];
        snprintf(tempStr, sizeof(tempStr), "Temp: %.1fC", temperature);
        snprintf(humStr, sizeof(humStr), "Hum: %.1f%%", humidity);
        updateDisplay("MQTT Active", tempStr, humStr);
        
        flashBlue();  // Flash main LED blue on successful send
        updateLEDs();  // Update discrete LEDs
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
    
    // Initialize discrete LEDs
    pinMode(LED_AZURE, OUTPUT);
    pinMode(LED_USER, OUTPUT);
    digitalWrite(LED_AZURE, LOW);
    digitalWrite(LED_USER, LOW);
    
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
    
    int result = connectMQTT();
    
    if (result != 0)
    {
        hasMqtt = false;
        updateLEDs();
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
    static unsigned long lastPublish = 0;
    unsigned long now = millis();
    
    // Check connection status
    if (!mqttClient.isConnected())
    {
        hasMqtt = false;
        updateLEDs();  // Will turn off Azure LED
        updateDisplay("Disconnected!", "Reconnecting...", MQTT_HOST);
        
        Serial.println("Connection lost! Reconnecting...");
        if (connectMQTT() == 0)
        {
            hasMqtt = true;
            updateLEDs();  // Will set LEDs appropriately
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
        publishTelemetry();
    }
}
