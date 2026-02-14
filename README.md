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
- **LSM6DSL** - 6-axis accelerometer and gyroscope
- **LIS2MDL** - 3-axis magnetometer

All sensors are automatically initialized by the framework via the global `Sensors` singleton.

Telemetry payload format:
```json
{
  "messageId": 42,
  "deviceId": "Device1",
  "temperature": 24.50,
  "humidity": 45.30,
  "pressure": 1013.25,
  "accelerometer": { "x": 10, "y": -5, "z": 980 },
  "gyroscope": { "x": 100, "y": -200, "z": 50 },
  "magnetometer": { "x": 150, "y": -300, "z": 500 }
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
│   └── config.h               # MQTT topics and timing intervals
├── platformio.ini             # PlatformIO configuration
└── README.md
```

## Configuration

The MXChip stores all connection settings (WiFi, certificates, broker URL, etc.) in the STSAFE secure element's EEPROM. You configure the device using the **Web Configuration UI** (recommended) or the serial **Configuration CLI**.

### Web Configuration UI (Recommended)

The web UI is the easiest way to configure the device — paste certificates directly without any special formatting.

1. Hold **Button B** while pressing **Reset** to enter AP mode
2. The OLED display shows the AP name (e.g., `AZ3166_XXXXXX`) and IP `192.168.0.1`
3. Connect your computer/phone to the device's WiFi access point
4. Open a browser and navigate to `http://192.168.0.1`
5. Fill in the form:
   - **Wi-Fi**: Select your network from the dropdown (or enter manually) and enter the password
   - **Broker URL**: Your MQTT broker endpoint (e.g., `mqtts://yournamespace.eastus-1.ts.eventgrid.azure.net:8883`)
   - **CA Certificate**: Paste the full PEM certificate including `-----BEGIN CERTIFICATE-----` and `-----END CERTIFICATE-----` lines
   - **Client Certificate**: Paste your device's client certificate (PEM)
   - **Client Private Key**: Paste your device's private key (PEM)
6. Click **Save Configuration** — the device will save to EEPROM and reboot

### Configuration CLI (Alternative)

For scripting or headless setups, use the serial console.

1. Hold **Button A** while pressing **Reset** to enter configuration mode
2. Connect via serial at 115200 baud
3. Enter commands:

```
set_wifissid MyNetwork
set_wifipwd MyPassword
set_broker mqtts://yournamespace.eastus-1.ts.eventgrid.azure.net:8883
```

4. For certificates and keys, values must be **quoted** and use `\n` for newlines:

```
set_cacert "-----BEGIN CERTIFICATE-----\nMIIC...base64...\n-----END CERTIFICATE-----"
set_clientcert "-----BEGIN CERTIFICATE-----\nMIIB...base64...\n-----END CERTIFICATE-----"
set_clientkey "-----BEGIN EC PRIVATE KEY-----\nMIGk...base64...\n-----END EC PRIVATE KEY-----"
```

To convert a PEM file to a single-line string suitable for pasting:

```bash
# Linux/macOS
awk 'NR>1{printf "\\n"} {printf "%s",$0}' cert.pem

# PowerShell
(Get-Content cert.pem -Raw).TrimEnd() -replace "`n","\n" -replace "`r",""
```

5. Use `status` to verify all settings are configured, and `exit` to reboot

### Application Settings

MQTT topics and timing intervals are set in `src/config.h`:

```cpp
static const char* PUBLISH_TOPIC = "testtopics/topic1";
static const char* SUBSCRIBE_TOPIC = "testtopics/topic1";
#define PUBLISH_INTERVAL_MS 5000
```

> **Note**: WiFi credentials, broker URL, and certificates are read from EEPROM at runtime. The `config.h` file is only for application-level settings like topics and intervals.

## Generating Certificates

You can generate self-signed certificates using OpenSSL for testing:
https://github.com/howardginsburg/CertificateGenerator

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

IP: 192.168.1.100
Connecting to broker.example.com:8883...
MQTT connected!
Subscribed to: testtopics/topic1
Ready!

[0] {"messageId":0,"deviceId":"Device1","temperature":24.50,"humidity":45.30,"pressure":1013.25,"accelerometer":{"x":10,"y":-5,"z":980},"gyroscope":{"x":100,"y":-200,"z":50},"magnetometer":{"x":150,"y":-300,"z":500}}
[1] {"messageId":1,"deviceId":"Device1","temperature":24.62,"humidity":44.80,"pressure":1013.30,"accelerometer":{"x":12,"y":-3,"z":978},"gyroscope":{"x":95,"y":-210,"z":55},"magnetometer":{"x":148,"y":-305,"z":502}}

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

Azure Event Grid uses **DigiCert Global Root G3** as its CA certificate.

#### Using the Web UI (Recommended)

1. Hold **Button B** + **Reset** to enter AP mode
2. Connect to the device's WiFi AP and open `http://192.168.0.1`
3. Fill in the form:

| Field | Value |
|-------|-------|
| **Wi-Fi** | Select your network and enter the password |
| **Broker URL** | `mqtts://yournamespace.eastus-1.ts.eventgrid.azure.net:8883` |
| **CA Certificate** | Paste the DigiCert Global Root G3 certificate below |
| **Client Certificate** | Paste your device's client certificate (PEM) |
| **Client Private Key** | Paste your device's private key (PEM) |

4. Click **Save Configuration**

**DigiCert Global Root G3** — paste this into the CA Certificate field:

```
-----BEGIN CERTIFICATE-----
MIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw
CQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu
ZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe
Fw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw
EwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x
IDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF
K4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG
fp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO
Z9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd
BgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx
AK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/
oAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8
sycX
-----END CERTIFICATE-----
```

#### Using the CLI (Alternative)

Hold **Button A** + **Reset**, connect at 115200 baud, then enter:

```
set_wifissid MyNetwork
set_wifipwd MyPassword
set_broker mqtts://yournamespace.eastus-1.ts.eventgrid.azure.net:8883
set_cacert "-----BEGIN CERTIFICATE-----\nMIICPzCCAcWgAwIBAgIQBVVWvPJepDU1w6QP1atFcjAKBggqhkjOPQQDAzBhMQsw\nCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3d3cu\nZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBHMzAe\nFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVTMRUw\nEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5jb20x\nIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEczMHYwEAYHKoZIzj0CAQYF\nK4EEACIDYgAE3afZu4q4C/sLfyHS8L6+c/MzXRq8NOrexpu80JX28MzQC7phW1FG\nfp4tn+6OYwwX7Adw9c+ELkCDnOg/QW07rdOkFFk2eJ0DQ+4QE2xy3q6Ip6FrtUPO\nZ9wj/wMco+I+o0IwQDAPBgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAd\nBgNVHQ4EFgQUs9tIpPmhxdiuNkHMEWNpYim8S8YwCgYIKoZIzj0EAwMDaAAwZQIx\nAK288mw/EkrRLTnDCgmXc/SINoyIJ7vmiI1Qhadj+Z4y3maTD/HMsQmP3Wyr+mt/\noAIwOWZbwmSNuJ5Q3KjVSaLtx9zRSX8XAbjIho9OjIgrqJqpisXRAL34VOKa5Vt8\nsycX\n-----END CERTIFICATE-----"
set_clientcert "<your client cert PEM with \n for newlines>"
set_clientkey "<your client key PEM with \n for newlines>"
exit
```

#### MQTT Topics

In `src/config.h`, set the MQTT topics:

```cpp
static const char* PUBLISH_TOPIC = "testtopics/topic1";
static const char* SUBSCRIBE_TOPIC = "testtopics/topic1";
```

> **Note**: The device ID (MQTT Client ID) is automatically extracted from the CN of your client certificate. It must match the Client authentication name configured in Event Grid.

### Verification Checklist

- [ ] CA certificate uploaded to Event Grid and shows "Valid" status
- [ ] Client created with Subject Matches validation
- [ ] Client certificate CN matches the Client's certificate subject exactly
- [ ] Client group includes your client (or use `$all`)
- [ ] Topic space includes your publish/subscribe topics
- [ ] Permission bindings grant Publisher and Subscriber access
- [ ] Device CLI `status` shows all settings configured (broker, CA cert, client cert, client key)

## License

MIT License
