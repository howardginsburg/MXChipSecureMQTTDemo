# MXChip AZ3166 Secure MQTT Demo

A demonstration of mutual TLS (mTLS) MQTT connectivity on the MXChip AZ3166 IoT DevKit using X.509 client certificate authentication.

## Important: Publish-Only Limitation

**This implementation supports PUBLISH operations only.** MQTT subscriptions and message receiving are not supported due to a limitation in the MXChip's TLS socket library.

### Technical Background

The MXChip AZ3166 uses an embedded mbedTLS implementation (last updated 2020) with the following behavior:

- When the MQTT `loop()` function attempts to read from the TLS socket during idle periods, the underlying mbedTLS library receives a `PEER_CLOSE_NOTIFY` or times out
- These read errors propagate up to PubSubClient, which interprets them as connection loss
- This triggers unnecessary reconnection cycles, even though the connection is actually still valid for publishing

### Workaround

The code intentionally skips calling `mqttClient.loop()` and only checks connection status when publishing. This provides stable publish-only operation for telemetry use cases.

## Features

- **Mutual TLS Authentication** - Both client and server present certificates
- **X.509 Client Certificates** - No username/password required
- **Automatic Reconnection** - Handles connection drops gracefully
- **JSON Telemetry** - Publishes simulated sensor data (temperature, humidity)
- **OLED Display** - Shows connection status, IP address, and telemetry data
- **LED Status Indicators** - Visual feedback for connection state

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
├── include/
│   ├── mqtt_config.h          # Your configuration (gitignored)
│   └── mqtt_config_sample.txt # Sample configuration template
├── src/
│   └── main.cpp               # Main application code
├── platformio.ini             # PlatformIO configuration
└── README.md
```

## Configuration

1. **Create the configuration file**

   ```bash
   cp include/mqtt_config_sample.txt include/mqtt_config.h
   ```

2. **Edit `include/mqtt_config.h`** with your values:
   - WiFi credentials (SSID and password)
   - MQTT broker hostname and port
   - Client ID (must match broker configuration)
   - PEM-formatted certificates

## Generating Certificates

```bash
# Generate private key
openssl genrsa -out client.key 2048

# Generate certificate signing request
openssl req -new -key client.key -out client.csr -subj "/CN=az3166-device-001"

# Sign with your CA
openssl x509 -req -in client.csr -CA ca.crt -CAkey ca.key -CAcreateserial -out client.crt -days 365
```

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
========================================
  MXChip Secure MQTT (Publish-Only)
========================================

Connecting to WiFi... Connected!
IP: 192.168.1.100

Connecting to MQTT broker...
  Broker: your-mqtt-broker.example.com
Connected!

Ready. Publishing every 5 seconds...

[1] Published
[2] Published
[3] Published
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

1. Create an Event Grid Namespace with MQTT enabled
2. Upload your CA certificate under **Client authentication** > **CA certificates**
3. Create a **Client** with certificate chain authentication
4. Create a **Topic space** (e.g., `testtopics/#`)
5. Create **Permission bindings** for publish access

## Framework Modifications

This project uses a modified Arduino framework for the MXChip. The following files were modified to enable stable TLS operation:

- `AZ3166WiFiClientSecure.cpp` - Modified read() to prevent spurious disconnections
- `TLSSocket.cpp` - Added close notify handling, reduced retry delays

See the [framework README](https://github.com/howardginsburg/framework-arduinostm32mxchip) for details.

## License

MIT License
