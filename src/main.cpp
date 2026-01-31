/**
 * @file main.cpp
 * @brief MQTT with mutual TLS (client certificate authentication)
 * 
 * This example demonstrates connecting to an MQTT broker using mutual TLS,
 * where both the server and client present certificates for authentication.
 * 
 * IMPORTANT: This implementation is PUBLISH-ONLY due to TLS library limitations.
 * See README.md for details on the TLS socket behavior with Azure Event Grid.
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
 */

#include <AZ3166WiFi.h>
#include "AZ3166WifiClientSecure.h"
#include <PubSubClient.h>
#include "OledDisplay.h"
#include "RGB_LED.h"
#include "mqtt_config.h"
#include <time.h>

// Hardware objects
RGB_LED rgbLed;

WiFiClientSecure wifiClient;
PubSubClient mqttClient(wifiClient);
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

/**
 * Update OLED display with up to 3 lines
 */
void updateDisplay(const char* line1, const char* line2 = NULL, const char* line3 = NULL)
{
    Screen.clean();
    Screen.print(0, line1);
    if (line2) Screen.print(1, line2);
    if (line3) Screen.print(2, line3);
}

/**
 * Update RGB and discrete LEDs based on connection status
 */
void updateLEDs()
{
    digitalWrite(LED_AZURE, hasMqtt ? HIGH : LOW);
    digitalWrite(LED_USER, (hasWifi && hasMqtt) ? HIGH : LOW);
    
    if (!hasWifi)
    {
        rgbLed.setColor(LED_RED);
    }
    else if (!hasMqtt)
    {
        rgbLed.setColor(LED_YELLOW);
    }
}

/**
 * Flash RGB LED blue to indicate successful publish
 */
void flashBlue()
{
    rgbLed.setColor(LED_BLUE);
    delay(200);
    rgbLed.turnOff();
}

/**
 * Connect to MQTT broker with mutual TLS
 * @return 0 on success, error code on failure
 */
int connectMQTT()
{
    Serial.println("\nConnecting to MQTT broker...");
    Serial.print("  Broker: ");
    Serial.println(MQTT_HOST);
    
    rgbLed.setColor(LED_YELLOW);

    // Configure TLS with certificates
    wifiClient.setTimeout(2000);
    wifiClient.setCACert(CA_CERT);
    wifiClient.setCertificate(CLIENT_CERT);
    wifiClient.setPrivateKey(CLIENT_KEY);

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setKeepAlive(60);

    // Connect with client ID (used as username for Azure Event Grid)
    bool connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_CLIENT_ID, "");

    if (!connected)
    {
        int state = mqttClient.state();
        char errStr[20];
        snprintf(errStr, sizeof(errStr), "Error: %d", state);
        updateDisplay("MQTT FAILED!", errStr, MQTT_HOST);
        Serial.print("Connection failed! Code: ");
        Serial.println(state);
        return state;
    }
    
    Serial.println("Connected!");
    
    // Publish immediately to establish the connection
    char testPayload[] = "{\"status\":\"connected\"}";
    mqttClient.publish(PUBLISH_TOPIC, testPayload);

    return 0;
}

/**
 * Build JSON payload with sensor data
 */
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

/**
 * Publish telemetry data to MQTT broker
 */
void publishTelemetry()
{
    float temperature = 20.0 + (random(200) / 10.0);
    float humidity = 40.0 + (random(400) / 10.0);
    
    char payload[256];
    buildPayload(payload, sizeof(payload), temperature, humidity);
    
    Serial.print("[");
    Serial.print(messageCount);
    Serial.print("] ");
    Serial.println(payload);
    
    bool result = mqttClient.publish(PUBLISH_TOPIC, payload);
    
    if (!result)
    {
        Serial.println("    Publish failed!");
        rgbLed.setColor(LED_RED);
        updateDisplay("Publish Error", "Failed");
    }
    else
    {
        char tempStr[20], humStr[20];
        snprintf(tempStr, sizeof(tempStr), "Temp: %.1fC", temperature);
        snprintf(humStr, sizeof(humStr), "Hum: %.1f%%", humidity);
        updateDisplay(WiFi.localIP().get_address(), tempStr, humStr);
        
        flashBlue();
        updateLEDs();
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
    
    pinMode(LED_AZURE, OUTPUT);
    pinMode(LED_USER, OUTPUT);
    digitalWrite(LED_AZURE, LOW);
    digitalWrite(LED_USER, LOW);
    
    Serial.println("\n========================================");
    Serial.println("  MXChip Secure MQTT (Publish-Only)");
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
        Serial.println("\nMQTT connection failed. Check certificates and broker config.");
        while (1) { delay(1000); }
    }
    
    hasMqtt = true;
    updateLEDs();
    
    Serial.println("\nReady. Publishing every 5 seconds...\n");
    updateDisplay("MQTT Connected", MQTT_CLIENT_ID, "Ready!");
}

void loop()
{
    static unsigned long lastPublish = 0;
    unsigned long now = millis();
    
    // NOTE: We intentionally do NOT call mqttClient.loop() here.
    // The MXChip's TLS library has issues with read timeouts that cause
    // spurious disconnections. For publish-only mode, we just publish
    // and reconnect if the socket is lost.
    
    // Publish every 5 seconds
    if (now - lastPublish >= 5000 || lastPublish == 0)
    {
        lastPublish = now;
        
        // Check if we need to reconnect
        if (!wifiClient.connected())
        {
            Serial.println("Reconnecting...");
            hasMqtt = false;
            updateLEDs();
            
            if (connectMQTT() == 0)
            {
                hasMqtt = true;
                updateLEDs();
            }
            else
            {
                return;
            }
        }
        
        publishTelemetry();
    }
}
