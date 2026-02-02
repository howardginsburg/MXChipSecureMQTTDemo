/**
 * @file main.cpp
 * @brief Secure MQTT Demo for MXChip AZ3166
 * 
 * Demonstrates mutual TLS connection to Azure Event Grid MQTT broker
 * with publish/subscribe messaging.
 */

#include <AZ3166WiFi.h>
#include "AZ3166WifiClientSecure.h"
#include <PubSubClient.h>
#include "OledDisplay.h"
#include "RGB_LED.h"
#include "config.h"
#include "HTS221Sensor.h"
#include "LPS22HBSensor.h"

// Configuration
#define PUBLISH_INTERVAL_MS   5000
#define WIFI_CHECK_INTERVAL   5000

// LED colors
#define LED_RED       255, 0, 0
#define LED_BLUE      0, 0, 255
#define LED_YELLOW    255, 255, 0
#define LED_CYAN      0, 255, 255

// Global objects
static RGB_LED rgbLed;
static WiFiClientSecure wifiClient;
static PubSubClient mqttClient(wifiClient);
static DevI2C *i2c;
static HTS221Sensor *tempHumSensor;
static LPS22HBSensor *pressureSensor;

// State
static int messageCount = 0;
static bool hasWifi = false;
static bool hasMqtt = false;

/**
 * Update OLED display
 */
void updateDisplay(const char* line1, const char* line2 = NULL, const char* line3 = NULL)
{
    Screen.clean();
    Screen.print(0, line1);
    if (line2) Screen.print(1, line2);
    if (line3) Screen.print(2, line3);
}

/**
 * Update LEDs based on connection status
 */
void updateLEDs()
{
    digitalWrite(LED_AZURE, hasMqtt ? HIGH : LOW);
    digitalWrite(LED_USER, (hasWifi && hasMqtt) ? HIGH : LOW);
    
    if (!hasWifi)
        rgbLed.setColor(LED_RED);
    else if (!hasMqtt)
        rgbLed.setColor(LED_YELLOW);
    else
        rgbLed.turnOff();
}

/**
 * MQTT message callback
 */
void messageCallback(char* topic, byte* payload, unsigned int length)
{
    Serial.printf("\n[Message Received] %s: ", topic);
    Serial.write(payload, length);
    Serial.println();
}

/**
 * Connect to MQTT broker with mutual TLS
 */
bool connectMQTT()
{
    Serial.printf("Connecting to %s:%d...\n", MQTT_HOST, MQTT_PORT);
    
    wifiClient.stop();
    rgbLed.setColor(LED_YELLOW);

    wifiClient.setTimeout(2000);
    wifiClient.setCACert(CA_CERT);
    wifiClient.setCertificate(CLIENT_CERT);
    wifiClient.setPrivateKey(CLIENT_KEY);

    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);

    if (!mqttClient.connect(MQTT_CLIENT_ID, MQTT_CLIENT_ID, ""))
    {
        Serial.printf("MQTT failed, state=%d\n", mqttClient.state());
        return false;
    }
    
    Serial.println("MQTT connected!");
    return true;
}

/**
 * Publish telemetry data
 */
void publishTelemetry()
{
    if (!mqttClient.connected()) return;
    
    float temp = 0, humidity = 0, pressure = 0;
    tempHumSensor->getTemperature(&temp);
    tempHumSensor->getHumidity(&humidity);
    pressureSensor->getPressure(&pressure);
    
    char payload[256];
    snprintf(payload, sizeof(payload),
        "{\"messageId\":%d,\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,\"deviceId\":\"%s\"}",
        messageCount++, temp, humidity, pressure, MQTT_CLIENT_ID);
    
    if (mqttClient.publish(PUBLISH_TOPIC, payload))
    {
        Serial.printf("[%d] %s\n", messageCount - 1, payload);
        
        char line2[20], line3[20];
        snprintf(line2, sizeof(line2), "T:%.1fC H:%.0f%%", temp, humidity);
        snprintf(line3, sizeof(line3), "P:%.0f hPa", pressure);
        updateDisplay(WiFi.localIP().get_address(), line2, line3);
        rgbLed.setColor(LED_BLUE);
        delay(100);
        rgbLed.turnOff();
    }
}

void setup()
{
    Serial.begin(115200);
    delay(500);
    
    Screen.init();
    rgbLed.turnOff();
    pinMode(LED_AZURE, OUTPUT);
    pinMode(LED_USER, OUTPUT);
    
    updateDisplay("Secure MQTT", "Initializing...");
    Serial.println("\n=== MXChip Secure MQTT Demo ===\n");
    
    // Initialize sensors
    i2c = new DevI2C(D14, D15);
    tempHumSensor = new HTS221Sensor(*i2c);
    tempHumSensor->init(NULL);
    tempHumSensor->enable();
    pressureSensor = new LPS22HBSensor(*i2c);
    pressureSensor->init(NULL);
    Serial.println("Sensors initialized");
    
    // Connect to WiFi
    updateDisplay("Connecting WiFi", WIFI_SSID);
    Serial.printf("Connecting to %s", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    
    for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++)
    {
        delay(500);
        Serial.print(".");
    }
    
    if (WiFi.status() != WL_CONNECTED)
    {
        updateDisplay("WiFi FAILED!", WIFI_SSID);
        Serial.println(" FAILED!");
        while (1) delay(1000);
    }
    
    hasWifi = true;
    Serial.printf(" OK\nIP: %s\n", WiFi.localIP().get_address());
    
    // Connect to MQTT
    updateDisplay("Connecting MQTT", MQTT_HOST);
    if (!connectMQTT())
    {
        updateDisplay("MQTT FAILED!", MQTT_HOST);
        while (1) delay(1000);
    }
    
    hasMqtt = true;
    updateLEDs();
    
    mqttClient.setCallback(messageCallback);
    mqttClient.subscribe(SUBSCRIBE_TOPIC);
    Serial.printf("Subscribed to: %s\n", SUBSCRIBE_TOPIC);
    
    updateDisplay("Ready", WiFi.localIP().get_address(), MQTT_CLIENT_ID);
    Serial.println("Ready!\n");
}

void loop()
{
    static unsigned long lastPublish = 0;
    static unsigned long lastWiFiCheck = 0;
    unsigned long now = millis();
    
    // Check WiFi periodically
    if (now - lastWiFiCheck >= WIFI_CHECK_INTERVAL)
    {
        lastWiFiCheck = now;
        hasWifi = (WiFi.status() == WL_CONNECTED);
        
        if (!hasWifi)
        {
            hasMqtt = false;
            updateLEDs();
            Serial.println("WiFi lost, reconnecting...");
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
            delay(500);
            return;
        }
    }
    
    if (!hasWifi)
    {
        delay(100);
        return;
    }
    
    // Handle MQTT
    if (mqttClient.connected())
    {
        hasMqtt = true;
        mqttClient.loop();
    }
    else
    {
        hasMqtt = false;
        updateLEDs();
        
        if (connectMQTT())
        {
            hasMqtt = true;
            updateLEDs();
            mqttClient.subscribe(SUBSCRIBE_TOPIC);
        }
        else
        {
            delay(2000);
            return;
        }
    }
    
    // Publish telemetry
    if (now - lastPublish >= PUBLISH_INTERVAL_MS)
    {
        lastPublish = now;
        publishTelemetry();
    }
    
    delay(10);
}
