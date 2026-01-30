# MXChip AZ3166 Secure MQTT Demo

A demonstration of mutual TLS (mTLS) MQTT connectivity on the MXChip AZ3166 IoT DevKit using X.509 client certificate authentication.

## Features

- **Mutual TLS Authentication** - Both client and server present certificates
- **X.509 Client Certificates** - No username/password required for authentication
- **Automatic Reconnection** - Handles connection drops gracefully
- **JSON Telemetry** - Publishes simulated sensor data (temperature, humidity)
- **Message Subscription** - Receives and displays incoming MQTT messages

## Prerequisites

- [PlatformIO](https://platformio.org/) IDE or CLI
- MXChip AZ3166 IoT DevKit
- MQTT broker with TLS and client certificate support (e.g., Mosquitto, Azure Event Grid, EMQX)
- X.509 certificates:
  - CA certificate (root or intermediate that signed the broker's cert)
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
   # Copy the sample configuration
   cp include/mqtt_config_sample.txt include/mqtt_config.h
   ```

2. **Edit `include/mqtt_config.h`** with your values:
   - WiFi credentials (SSID and password)
   - MQTT broker hostname and port
   - Client ID (must match broker configuration)
   - PEM-formatted certificates (include `\n` for line breaks)

## Generating Certificates

Use OpenSSL to generate a client certificate:

```bash
# Generate private key
openssl genrsa -out client.key 2048

# Generate certificate signing request
openssl req -new -key client.key -out client.csr -subj "/CN=az3166-device-001"

# Sign with your CA (adjust paths to your CA cert/key)
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

Or use PlatformIO IDE's build/upload buttons.

## Serial Output

On successful connection:

```
========================================
  Mutual TLS MQTT Example
  Client Certificate Authentication
========================================

Connecting to WiFi... Connected!
IP: 192.168.1.100

Establishing mutual TLS connection...
  Broker: your-mqtt-broker.example.com
  Port: 8883
  Client ID: az3166-device-001
  Auth: Client Certificate (X.509)

*** Connected with Mutual TLS! ***

Subscribing to: commands/az3166/#
Subscribed successfully!

Ready. Publishing every 30 seconds...

[1] Publishing: {"messageId":0,"temperature":25.30,"humidity":55.20,"timestamp":30}
    Sent successfully
```

## Troubleshooting

### MQTT Error Codes

| Code | Meaning |
|------|---------|
| 1 | Unacceptable protocol version |
| 2 | Client identifier rejected |
| 3 | Server unavailable |
| 4 | Bad username or password |
| 5 | Not authorized |
| Negative | Network or TLS error |

### Common Issues

**Error 5 (Not authorized)**
- Verify client certificate CN matches the client ID registered with the broker
- Check that the CA certificate is registered with the broker
- For Azure Event Grid: ensure client is created and permission bindings are configured

**TLS Handshake Failure (negative error)**
- Verify CA certificate is correct for the broker
- Check certificate expiration dates
- Ensure private key matches the client certificate

**WiFi Connection Failed**
- Verify SSID and password
- Ensure 2.4GHz network (MXChip doesn't support 5GHz)

## Azure Event Grid MQTT

For Azure Event Grid MQTT broker:

1. Create an Event Grid Namespace with MQTT enabled
2. Upload your CA certificate under **Client authentication** > **CA certificates**
3. Create a **Client** with:
   - Authentication type: Certificate chain
   - Client authentication name matching your certificate CN
4. Create a **Topic space** (e.g., `testtopics/#`)
5. Create **Permission bindings** to grant publish/subscribe access

## License

MIT License