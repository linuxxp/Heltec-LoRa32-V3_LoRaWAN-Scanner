# LoRaWAN Signal Scanner

A LoRaWAN coverage mapping device based on Heltec LoRa32 V3 with GPS.

## Features

- **GPS-based signal mapping** - Sends GPS coordinates via LoRaWAN
- **Helium network support** - Designed for Helium LoRaWAN network
- **Multiple operation modes**:
  - **Manual** - Button-triggered measurements
  - **Continuous** - Periodic measurements at configurable intervals
  - **Auto** - Automatic measurements based on movement distance
  - **Deep Sleep** - Low power mode with periodic wake-up
- **OLED display** - Shows GPS status, network info, and settings
- **Single-button navigation** - Full menu control with one button
- **Battery monitoring** - Shows battery percentage and voltage

## Hardware

- **Heltec LoRa32 V3** (ESP32-S3 + SX1262)
- **GT-U7 GPS Module** (or compatible)
- **Li-ion Battery** (3.7V, JST connector)

### Wiring

| GT-U7 GPS | Heltec V3 | Description |
|-----------|-----------|-------------|
| VCC       | 3.3V      | Power supply |
| GND       | GND       | Ground |
| TXD       | GPIO48    | GPS TX → ESP32 RX |
| RXD       | GPIO47    | GPS RX ← ESP32 TX |
| EN        | GPIO46    | Enable (HIGH=on) |

## Setup

### 1. Install PlatformIO

Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO extension](https://platformio.org/install/ide?install=vscode).

### 2. Configure Credentials

Copy `include/credentials.h.example` to `include/credentials.h` and fill in your Helium Console credentials:

```cpp
// Device EUI (8 bytes, LSB first)
static const uint8_t DEVEUI[8] = { 0xXX, 0xXX, ... };

// Application EUI (8 bytes, LSB first)
static const uint8_t APPEUI[8] = { 0xXX, 0xXX, ... };

// Application Key (16 bytes, MSB)
static const uint8_t APPKEY[16] = { 0xXX, 0xXX, ... };
```

### 3. Build and Upload

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor
```

## Button Controls

| Action | Duration | Function |
|--------|----------|----------|
| Single click | <300ms | Next screen / Navigate menu |
| Double click | 2× <300ms | Trigger TX / Select menu item |
| Long press | >1s | Enter/Exit menu |
| Very long press | >3s | Force LoRa TX (debug) |

## Payload Format

14 bytes, big-endian:

| Bytes | Field | Type | Description |
|-------|-------|------|-------------|
| 0-3 | Latitude | int32 | ×10^7 |
| 4-7 | Longitude | int32 | ×10^7 |
| 8-9 | Altitude | int16 | meters |
| 10 | HDOP | uint8 | ×10 |
| 11 | Satellites | uint8 | count |
| 12 | Battery | uint8 | % |
| 13 | TX Power | int8 | dBm |

## Helium Console Decoder

Add this JavaScript decoder in your Helium Console:

```javascript
function Decoder(bytes, port) {
    var decoded = {};

    if (port === 1 && bytes.length >= 14) {
        // Latitude
        var lat = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        if (lat > 0x7FFFFFFF) lat -= 0x100000000;
        decoded.latitude = lat / 1e7;

        // Longitude
        var lon = (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7];
        if (lon > 0x7FFFFFFF) lon -= 0x100000000;
        decoded.longitude = lon / 1e7;

        // Altitude
        var alt = (bytes[8] << 8) | bytes[9];
        if (alt > 0x7FFF) alt -= 0x10000;
        decoded.altitude = alt;

        // Other fields
        decoded.hdop = bytes[10] / 10;
        decoded.satellites = bytes[11];
        decoded.battery = bytes[12];

        var txp = bytes[13];
        if (txp > 127) txp -= 256;
        decoded.txPower = txp;
    }

    return decoded;
}
```

## Data Collection

The device sends GPS coordinates. Signal quality (RSSI, SNR) is automatically added by Helium network metadata, including:

- **RSSI** - Received Signal Strength Indicator (dBm)
- **SNR** - Signal-to-Noise Ratio (dB)
- **Gateway info** - Which hotspot received the packet
- **Spreading Factor** - SF used for transmission
- **Frequency** - Channel frequency

Use a webhook integration in Helium Console to forward data to your backend (InfluxDB, MQTT, etc.).

## License

MIT License
