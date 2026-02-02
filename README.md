# MXChip AZ3166 Secure MQTT Demo

A demonstration of mutual TLS (mTLS) MQTT connectivity on the MXChip AZ3166 IoT DevKit using X.509 client certificate authentication with full publish/subscribe support.

## Features

- **Mutual TLS Authentication** - Both client and server present certificates
- **X.509 Client Certificates** - No username/password required  
- **Bidirectional MQTT** - Full publish and subscribe support
- **Real Sensor Data** - Temperature, humidity, and pressure from onboard sensors
- **Automatic Reconnection** - Handles WiFi and MQTT connection drops gracefully
- **JSON Telemetry** - Publishes sensor data every 5 seconds
- **OLED Display** - Shows connection status, IP address, and telemetry data
- **LED Status Indicators** - Visual feedback for connection state

## Sensors

The MXChip AZ3166 includes:
- **HTS221** - Temperature and humidity sensor
- **LPS22HB** - Barometric pressure sensor

Telemetry payload format:
```json
{
  "messageId": 42,
  "temperature": 24.50,
  "humidity": 45.30,
  "pressure": 1013.25,
  "deviceId": "Device1"
}
```

## Hardware Features

### OLED Display

The 128x64 OLED screen shows:
- Startup and connection progress
- IP address when connected
- Last published sensor values
- Error messages with codes

### LED Indicators

| LED | State | Meaning |
|-----|-------|---------|
| **RGB LED** | Yellow | Connecting |
| | Blue flash | Message published |
| | Red | Error/Disconnected |
| **Azure LED** | On | MQTT connected |
| | Off | MQTT disconnected |
| **User LED** | On | WiFi + MQTT connected |

## Prerequisites

- [PlatformIO](https://platformio.org/) IDE or CLI
- MXChip AZ3166 IoT DevKit
- MQTT broker with TLS and client certificate support (e.g., Azure Event Grid, Mosquitto)
- X.509 certificates:
  - CA certificate (root that signed the broker's cert)
  - Client certificate
  - Client private key

## Project Structure

```
MXChipSecureMQTTDemo/
├── src/
│   ├── main.cpp               # Main application code
│   ├── config.h               # Your configuration (gitignored)
│   └── config.sample.txt      # Sample configuration template
├── platformio.ini             # PlatformIO configuration
├── TLSPATCH.md                # Framework patch instructions
└── README.md
```

## Configuration

1. **Create the configuration file**

   ```bash
   cp src/config.sample.txt src/config.h
   ```

2. **Edit `src/config.h`** with your values:
   - WiFi credentials (SSID and password)
   - MQTT broker hostname and port
   - Client ID (must match broker configuration)
   - PEM-formatted certificates

## Generating Certificates

You can generate self-signed certificates using OpenSSL for testing:
https://github.com/howardginsburg/CertificateGenerator

```bash

## Building and Uploading

```bash
# Build the project
pio run

# Upload to the device
pio run --target upload

# Monitor serial output
pio device monitor
```

## Serial Output

```
=== MXChip Secure MQTT Demo ===

Sensors initialized
Connecting to MyWiFi... OK
IP: 192.168.1.100
Connecting to broker.example.com:8883...
MQTT connected!
Subscribed to: testtopics/topic1
Ready!

[0] {"messageId":0,"temperature":24.50,"humidity":45.30,"pressure":1013.25,"deviceId":"Device1"}
[1] {"messageId":1,"temperature":24.62,"humidity":44.80,"pressure":1013.30,"deviceId":"Device1"}

[Message Received] testtopics/topic1: {"command":"hello"}
```

## Troubleshooting

### MQTT Error Codes

| Code | Meaning |
|------|---------|
| -4 | Connection timeout |
| -3 | Connection lost |
| -2 | Connect failed |
| -1 | Disconnected |
| 0 | Connected |
| 5 | Not authorized |

### Common Issues

**Error 5 (Not authorized)**
- Verify client certificate CN matches the client ID
- Check CA certificate is registered with the broker
- For Azure Event Grid: ensure permission bindings are configured

**TLS Handshake Failure**
- Verify CA certificate is correct
- Check certificate expiration dates
- Ensure private key matches client certificate

**WiFi Connection Failed**
- Verify SSID and password
- Ensure 2.4GHz network (MXChip doesn't support 5GHz)

## Azure Event Grid MQTT Setup

Azure Event Grid provides a fully managed MQTT broker with X.509 certificate authentication. Follow these steps to configure it for this demo.

### 1. Create Event Grid Namespace

1. In Azure Portal, create a new **Event Grid Namespace**
2. Under **Configuration**, enable **MQTT broker**
3. Note the **MQTT hostname** (e.g., `yournamespace.eastus-1.ts.eventgrid.azure.net`)

### 2. Upload CA Certificate

1. Navigate to **Client authentication** > **CA certificates**
2. Click **+ Add CA certificate**
3. Upload your root CA certificate (PEM format)
4. Give it a name (e.g., `DemoRootCA`)

### 3. Create a Client (Device Registration)

1. Go to **Clients** > **+ Add client**
2. Configure the client:

| Field | Value | Description |
|-------|-------|-------------|
| **Client name** | `Device1` | Logical name for the device |
| **Authentication name** | `Device1` | Must match the MQTT Client ID |
| **Client certificate authentication** | Enabled | Use X.509 certificates |
| **Validation scheme** | Subject Matches | Match certificate subject |
| **Certificate subject** | `CN=Device1` | Must match the CN in client certificate |

**Subject Matches Authentication:**
- The client certificate's Subject (CN) is validated against the configured value
- Format: `CN=YourDeviceName` or full DN like `CN=Device1,O=Contoso,C=US`
- This ensures only certificates with matching subjects can connect as this client

### 4. Create Client Groups

Client groups allow you to organize clients and assign permissions collectively.

1. Go to **Client groups** > **+ Add client group**
2. Create a group (e.g., `AllDevices`)
3. Add a query to match clients:
   - **Query**: `attributes.type = "sensor"` (if using attributes)
   - Or use `$all` to match all clients

**Built-in Groups:**
- `$all` - Matches all registered clients

### 5. Create Topic Spaces

Topic spaces define which topics clients can publish/subscribe to.

1. Go to **Topic spaces** > **+ Add topic space**
2. Configure the topic space:

| Field | Value |
|-------|-------|
| **Name** | `TestTopics` |
| **Topic templates** | `testtopics/#` |

**Topic Template Patterns:**
- `testtopics/#` - All topics under testtopics/
- `devices/${client.name}/telemetry` - Client-specific topics using variables
- `commands/+/action` - Single-level wildcard

**Supported Variables:**
- `${client.name}` - The client's authentication name
- `${client.attributes.xxx}` - Client attributes

### 6. Create Permission Bindings

Permission bindings connect client groups to topic spaces with specific permissions.

1. Go to **Permission bindings** > **+ Add permission binding**
2. Create bindings for publish and subscribe:

**Publisher Binding:**
| Field | Value |
|-------|-------|
| **Name** | `AllDevicesCanPublish` |
| **Client group** | `$all` |
| **Topic space** | `TestTopics` |
| **Permission** | Publisher |

**Subscriber Binding:**
| Field | Value |
|-------|-------|
| **Name** | `AllDevicesCanSubscribe` |
| **Client group** | `$all` |
| **Topic space** | `TestTopics` |
| **Permission** | Subscriber |

### 7. Update Device Configuration

In `src/config.h`, set:

```cpp
static const char* MQTT_HOST = "yournamespace.eastus-1.ts.eventgrid.azure.net";
static const int MQTT_PORT = 8883;
static const char* MQTT_CLIENT_ID = "Device1";  // Must match Client authentication name
static const char* PUBLISH_TOPIC = "testtopics/topic1";
static const char* SUBSCRIBE_TOPIC = "testtopics/topic1";
```

### Verification Checklist

- [ ] CA certificate uploaded and shows "Valid" status
- [ ] Client created with Subject Matches validation
- [ ] Certificate CN matches the Client's certificate subject exactly
- [ ] Client group includes your client (or use `$all`)
- [ ] Topic space includes your publish/subscribe topics
- [ ] Permission bindings grant Publisher and Subscriber access
- [ ] MQTT_CLIENT_ID in config matches Client authentication name

## License

MIT License
