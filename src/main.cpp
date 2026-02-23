/**
 * @file main.cpp
 * @brief Secure MQTT Demo for MXChip AZ3166
 * 
 * Demonstrates mutual TLS connection to Azure Event Grid MQTT broker
 * with publish/subscribe messaging. Configuration is loaded from EEPROM
 * using the DeviceConfig framework.
 */

#include <AZ3166WiFi.h>
#include <PubSubClient.h>
#include "OledDisplay.h"
#include "RGB_LED.h"
#include "DeviceConfig.h"
#include "SensorManager.h"
#include "SystemTime.h"
#include <time.h>

#if CONNECTION_PROFILE == PROFILE_MQTT_USERPASS
  #include "AZ3166WiFiClient.h"
  static WiFiClient wifiClient;
#else
  #include "AZ3166WifiClientSecure.h"
  static WiFiClientSecure wifiClient;
#endif
// Global objects
static RGB_LED rgbLed;
static PubSubClient mqttClient(wifiClient);

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
        rgbLed.setRed();
    else if (!hasMqtt)
        rgbLed.setYellow();
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
 * Connect to MQTT broker (profile-dependent: userpass, userpass+TLS, or mTLS)
 */
bool connectMQTT()
{
    const char* host = DeviceConfig_GetBrokerHost();
    int port = DeviceConfig_GetBrokerPort();
    
    Serial.printf("Connecting to %s:%d...\n", host, port);
    
    wifiClient.stop();
    rgbLed.setYellow();

#if CONNECTION_PROFILE == PROFILE_MQTT_USERPASS
    // Plain TCP — no TLS configuration needed
#elif CONNECTION_PROFILE == PROFILE_MQTT_USERPASS_TLS
    wifiClient.setTimeout(2000);
    wifiClient.setCACert(DeviceConfig_GetCACert());
#elif CONNECTION_PROFILE == PROFILE_MQTT_MTLS
    wifiClient.setTimeout(2000);
    wifiClient.setCACert(DeviceConfig_GetCACert());
    wifiClient.setCertificate(DeviceConfig_GetClientCert());
    wifiClient.setPrivateKey(DeviceConfig_GetClientKey());
#endif

    mqttClient.setServer(host, port);
    mqttClient.setBufferSize(1024);
    mqttClient.setKeepAlive(60);
    mqttClient.setSocketTimeout(30);

    const char* deviceId = DeviceConfig_GetDeviceId();

#if CONNECTION_PROFILE == PROFILE_MQTT_USERPASS || CONNECTION_PROFILE == PROFILE_MQTT_USERPASS_TLS
    char devicePassword[680];
    DeviceConfig_Read(SETTING_DEVICE_PASSWORD, devicePassword, sizeof(devicePassword));
    if (!mqttClient.connect(deviceId, deviceId, devicePassword))
#else
    if (!mqttClient.connect(deviceId, deviceId, ""))
#endif
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
    
    char sensorJson[512];
    if (!Sensors.toJson(sensorJson, sizeof(sensorJson))) return;
    
    // Get ISO 8601 timestamp
    char timestamp[25];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    
    // Build final payload with messageId, deviceId, timestamp, and all sensor data
    char payload[700];
    snprintf(payload, sizeof(payload),
        "{\"messageId\":%d,\"deviceId\":\"%s\",\"timestamp\":\"%s\",%s",
        messageCount++, DeviceConfig_GetDeviceId(), timestamp, sensorJson + 1);  // skip leading '{' to merge objects
    
    const char* publishTopic = DeviceConfig_GetPublishTopic();
    if (publishTopic[0] == '\0') return;

    if (mqttClient.publish(publishTopic, payload))
    {
        Serial.printf("[%d] %s\n", messageCount - 1, payload);
        
        float temp = Sensors.getTemperature();
        float hum = Sensors.getHumidity();
        float pres = Sensors.getPressure();
        
        char line2[20], line3[20];
        snprintf(line2, sizeof(line2), "T:%.1fC H:%.0f%%", temp, hum);
        snprintf(line3, sizeof(line3), "P:%.0f hPa", pres);
        updateDisplay(WiFi.localIP().get_address(), line2, line3);
        rgbLed.setBlue();
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
    
    updateDisplay("MQTT", "Initializing...");
    Serial.println("\n=== MXChip MQTT Demo ===\n");
    Serial.printf("Profile:          %s\n", DeviceConfig_GetProfileName());
    Serial.printf("WiFi SSID:        %s\n", DeviceConfig_GetWifiSsid());
    Serial.printf("WiFi password len:%d\n", (int)strlen(DeviceConfig_GetWifiPassword()));
    Serial.printf("Broker host:      %s\n", DeviceConfig_GetBrokerHost());
    Serial.printf("Broker port:      %d\n", DeviceConfig_GetBrokerPort());
    Serial.printf("Device ID:        %s\n", DeviceConfig_GetDeviceId());
    Serial.printf("Send interval:    %d s\n", DeviceConfig_GetSendInterval());
    Serial.printf("Publish topic:    \"%s\"\n", DeviceConfig_GetPublishTopic());
    Serial.printf("Subscribe topic:  \"%s\"\n", DeviceConfig_GetSubscribeTopic());
#if CONNECTION_PROFILE == PROFILE_MQTT_USERPASS || CONNECTION_PROFILE == PROFILE_MQTT_USERPASS_TLS
    {
        char _pw[680];
        DeviceConfig_Read(SETTING_DEVICE_PASSWORD, _pw, sizeof(_pw));
        Serial.printf("Device password len:%d\n", (int)strlen(_pw));
    }
#endif
#if CONNECTION_PROFILE == PROFILE_MQTT_USERPASS_TLS || CONNECTION_PROFILE == PROFILE_MQTT_MTLS
    Serial.printf("CA cert len:      %d\n", (int)strlen(DeviceConfig_GetCACert()));
#endif
#if CONNECTION_PROFILE == PROFILE_MQTT_MTLS
    Serial.printf("Client cert len:  %d\n", (int)strlen(DeviceConfig_GetClientCert()));
    Serial.printf("Client key len:   %d\n", (int)strlen(DeviceConfig_GetClientKey()));
#endif
    
    // Connect to WiFi (uses EEPROM credentials via DeviceConfig)
    updateDisplay("Connecting WiFi", DeviceConfig_GetWifiSsid());
    if (WiFi.begin() != WL_CONNECTED)
    {
        updateDisplay("WiFi FAILED!", DeviceConfig_GetWifiSsid());
        while (1) delay(1000);
    }
    
    hasWifi = true;
    Serial.printf("IP: %s\n", WiFi.localIP().get_address());
    
    // Sync time via NTP
    updateDisplay("Syncing time...");
    SyncTime();
    
    // Connect to MQTT
    updateDisplay("Connecting MQTT", DeviceConfig_GetBrokerHost());
    if (!connectMQTT())
    {
        updateDisplay("MQTT FAILED!", DeviceConfig_GetBrokerHost());
        while (1) delay(1000);
    }
    
    hasMqtt = true;
    updateLEDs();
    
    if (DeviceConfig_GetSubscribeTopic()[0] != '\0')
    {
        mqttClient.setCallback(messageCallback);
        mqttClient.subscribe(DeviceConfig_GetSubscribeTopic());
        Serial.printf("Subscribed to: %s\n", DeviceConfig_GetSubscribeTopic());
    }
    
    updateDisplay("Ready", WiFi.localIP().get_address(), DeviceConfig_GetDeviceId());
    Serial.println("Ready!\n");
}

void loop()
{
    static unsigned long lastPublish = 0;
    static unsigned long lastWiFiCheck = 0;
    unsigned long now = millis();
    
    // Check WiFi periodically
    if (now - lastWiFiCheck >= 5000)
    {
        lastWiFiCheck = now;
        hasWifi = (WiFi.status() == WL_CONNECTED);
        
        if (!hasWifi)
        {
            hasMqtt = false;
            updateLEDs();
            Serial.println("WiFi lost, reconnecting...");
            WiFi.begin();
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
        if (DeviceConfig_GetSubscribeTopic()[0] != '\0')
            mqttClient.subscribe(DeviceConfig_GetSubscribeTopic());
        }
        else
        {
            delay(2000);
            return;
        }
    }
    
    // Publish telemetry
    if (now - lastPublish >= (unsigned long)DeviceConfig_GetSendInterval() * 1000)
    {
        lastPublish = now;
        publishTelemetry();
    }
    
    delay(10);
}
