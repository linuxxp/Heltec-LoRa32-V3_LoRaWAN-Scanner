# LoRaWAN Signal Scanner

A portable LoRaWAN coverage mapping device based on **Heltec LoRa32 V3** with GPS. Designed for the **Helium Network**, this device sends GPS coordinates via LoRaWAN to map network coverage and signal quality.

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32--S3-green.svg)
![LoRaWAN](https://img.shields.io/badge/LoRaWAN-EU868-orange.svg)

## Features

- **GPS-based signal mapping** - Sends precise GPS coordinates via LoRaWAN
- **Helium network optimized** - Designed specifically for Helium LoRaWAN network
- **Four operation modes**:
  - **Manual** - Button-triggered measurements
  - **Continuous** - Periodic measurements at configurable intervals (10-300s)
  - **Auto** - Automatic measurements based on movement distance (10-500m)
  - **Deep Sleep** - Ultra low power mode with periodic wake-up
- **OLED display** - Shows GPS status, network info, battery, and settings
- **Single-button navigation** - Full menu control with short/long press
- **Battery monitoring** - Real-time percentage and voltage display
- **Credential generation** - Auto-generate unique LoRaWAN keys from ESP32 MAC or use manual credentials
- **QR code registration** - Scan QR code to get device credentials for Helium Console
- **NVS persistence** - Settings saved across reboots
- **Deep sleep support** - Button and timer wake-up sources

## Table of Contents

- [Hardware Requirements](#hardware-requirements)
- [Wiring Diagram](#wiring-diagram)
- [Software Setup](#software-setup)
- [Configuration](#configuration)
- [Operation Modes](#operation-modes)
- [Button Controls](#button-controls)
- [Display Screens](#display-screens)
- [Settings Menu](#settings-menu)
- [Helium Console Setup](#helium-console-setup)
- [Payload Format](#payload-format)
- [Helium Decoder](#helium-decoder)
- [Deep Sleep Mode](#deep-sleep-mode)
- [LED Patterns](#led-patterns)
- [Troubleshooting](#troubleshooting)
- [Technical Specifications](#technical-specifications)
- [License](#license)

## Hardware Requirements

### Main Components

| Component | Description |
|-----------|-------------|
| **Heltec LoRa32 V3** | ESP32-S3 + SX1262 LoRa radio + OLED display |
| **GT-U7 GPS Module** | u-blox compatible GPS with serial output |
| **Li-ion Battery** | 3.7V 18650 or LiPo (JST 1.25mm connector) |

### Heltec LoRa32 V3 Specifications

- **MCU**: ESP32-S3 (dual-core 240MHz, 8MB Flash, 8MB PSRAM)
- **LoRa**: Semtech SX1262
- **Display**: 0.96" OLED SSD1306 (128x64 pixels)
- **USB**: CP2102 USB-to-UART (not native USB CDC)
- **Battery**: Built-in charger and monitoring circuit
- **LED**: White LED on GPIO35

## Wiring Diagram

### GPS Module Connection (GT-U7)

| GT-U7 Pin | Heltec V3 Pin | Description |
|-----------|---------------|-------------|
| VCC | 3.3V | Power supply (3.3V recommended) |
| GND | GND | Ground |
| TXD | GPIO48 | GPS TX → ESP32 RX |
| RXD | GPIO47 | GPS RX ← ESP32 TX (optional) |
| EN | GPIO46 | Enable pin (HIGH = on) |

### Pin Assignment Summary

| Function | GPIO | Notes |
|----------|------|-------|
| **LoRa SX1262** | | |
| NSS (CS) | 8 | Chip select |
| SCK | 9 | SPI clock |
| MOSI | 10 | SPI data out |
| MISO | 11 | SPI data in |
| RST | 12 | Radio reset |
| BUSY | 13 | Radio busy signal |
| DIO1 | 14 | Interrupt |
| **OLED Display** | | |
| SDA | 17 | I2C data |
| SCL | 18 | I2C clock |
| RST | 21 | Display reset |
| **Other** | | |
| User Button | 0 | Active LOW, internal pull-up |
| LED | 35 | White LED |
| VEXT Control | 36 | Display power (LOW = on) |
| Battery ADC | 1 | Voltage sensing |
| Battery Control | 37 | ADC FET control |
| GPS RX | 48 | GPS data input |
| GPS TX | 47 | GPS data output |
| GPS EN | 46 | GPS enable |

## Software Setup

### Prerequisites

1. Install [Visual Studio Code](https://code.visualstudio.com/)
2. Install [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)

### Build and Upload

```bash
# Clone the repository
git clone https://github.com/yourusername/Heltec-LoRa32-V3_LoRaWAN-Scanner.git
cd Heltec-LoRa32-V3_LoRaWAN-Scanner

# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output (115200 baud)
pio device monitor
```

### First Boot

1. Connect the GPS module and battery
2. Power on the device via USB or battery
3. Wait for the boot screen showing "LoRa Scanner v1.0.0"
4. Device will attempt to join the LoRaWAN network
5. Navigate to **Settings → Show QR** to get credentials for Helium Console

## Configuration

### LoRaWAN Credentials

The device supports two credential modes, configured in `include/config.h`:

#### Option 1: Manual Credentials (Recommended for Helium)

Set `USE_MANUAL_CREDENTIALS` to `true` and enter your Helium Console credentials:

```cpp
#define USE_MANUAL_CREDENTIALS  true

// From Helium Console (MSB format)
#define MANUAL_DEV_EUI    { 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX }
#define MANUAL_JOIN_EUI   { 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX }
#define MANUAL_APP_KEY    { 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, \
                            0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX, 0xXX }
```

#### Option 2: Auto-Generated Credentials

Set `USE_MANUAL_CREDENTIALS` to `false`. The device will generate unique credentials from its ESP32 MAC address. Use the QR code feature to get the credentials for registration.

### Configurable Parameters

| Parameter | Location | Default | Range | Description |
|-----------|----------|---------|-------|-------------|
| `DEFAULT_MODE` | config.h | AUTO | - | Startup operation mode |
| `CONTINUOUS_INTERVAL_*` | config.h | 30s | 10-300s | Continuous mode interval |
| `AUTO_DISTANCE_*` | config.h | 50m | 10-500m | Auto mode trigger distance |
| `DEEP_SLEEP_INTERVAL_*` | config.h | 300s | 60-3600s | Deep sleep wake interval |
| `LORA_DEFAULT_SF` | config.h | SF7 | 7-12 | Spreading factor |
| `LORA_DEFAULT_POWER` | config.h | 14dBm | 2-14 | TX power (EU868 max: 14) |
| `GPS_MIN_SATELLITES` | config.h | 4 | - | Minimum satellites for valid fix |
| `GPS_MAX_HDOP` | config.h | 5.0 | - | Maximum HDOP for valid fix |
| `DISPLAY_TIMEOUT_MS` | config.h | 300000 | - | Display auto-off (5 min) |

## Operation Modes

### 1. Manual Mode (MANUAL)

- **Trigger**: Press button on Status screen
- **Use case**: Controlled measurements at specific locations
- **Behavior**: Only sends data when manually triggered

### 2. Continuous Mode (CONT)

- **Trigger**: Automatic at configured interval
- **Use case**: Stationary monitoring or slow-moving surveys
- **Interval**: Configurable 10-300 seconds
- **Behavior**: Sends data periodically regardless of movement

### 3. Auto Mode (AUTO) - Default

- **Trigger**: When device moves configured distance from last TX
- **Use case**: Walking or driving surveys
- **Distance**: Configurable 10-500 meters
- **Behavior**: Only sends when GPS detects sufficient movement

### 4. Deep Sleep Mode (SLEEP)

- **Trigger**: Timer wakeup at configured interval
- **Use case**: Long-term fixed installations, battery optimization
- **Wake sources**:
  - Timer (60-3600 seconds)
  - Button press (exits to Manual mode)
- **Power consumption**: ~10μA during sleep
- **Behavior**: Wake → GPS fix → Join → TX → Sleep

## Button Controls

The device uses a single button (GPIO0) for all navigation:

### On Main Screens

| Action | Duration | Function |
|--------|----------|----------|
| **Short press** | < 2 sec | Next screen |
| **Long press** | ≥ 2 sec | Enter settings menu |

### In Settings Menu

| Action | Duration | Function |
|--------|----------|----------|
| **Short press** | < 2 sec | Next menu item |
| **Long press** | ≥ 2 sec | Select/edit item or execute action |

### In Edit Mode

| Action | Duration | Function |
|--------|----------|----------|
| **Short press** | < 2 sec | Change value (cycle options) |
| **Long press** | ≥ 2 sec | Confirm and exit edit mode |

### Wake from Sleep

| Action | Function |
|--------|----------|
| **Any press** | Wake display (if timed out) |
| **Button press during deep sleep** | Exit deep sleep to Manual mode |

## Display Screens

The device has 5 main screens accessible by short-pressing the button:

### 1. Status Screen (Main)

```
LoRa Scanner    G■ L■ [75%]
─────────────────────────
Mode: AUTO
GPS: 42.6977,23.3219
LoRa: SF7 14dBm
TX: 5s ago OK #42
```

### 2. GPS Details Screen

```
GPS Details     G■ L■ [75%]
─────────────────────────
Lat: 42.697700
Lon: 23.321900
Alt: 550.5m
Sats:8 HDOP:1.2      FIX
```

### 3. Network Statistics Screen

```
Network Stat    G■ L■ [75%]
─────────────────────────
Status: Joined
TX Count: 42
TX Failed: 2
Success: 95.2%
```

### 4. Device Info Screen

```
Device Info     G■ L■ [75%]
─────────────────────────
LoRa-Scanner
FW: 1.0.0
Up: 01:23:45
Bat: 3850mV
```

### 5. QR Code Screen

Displays a QR code containing LoRaWAN credentials in format:
```
LORA:DevEUI;JoinEUI;AppKey
```

## Settings Menu

Long-press on any main screen to enter settings:

| Menu Item | Type | Description |
|-----------|------|-------------|
| **Mode** | Edit | Select operation mode (MANUAL/CONT/AUTO/SLEEP) |
| **Interval** | Edit | Continuous mode interval (10-300 sec) |
| **TX Power** | Edit | Transmit power (2-14 dBm, 2 dBm steps) |
| **SF** | Edit | Spreading Factor (SF7-SF12) |
| **GPS Dist** | Edit | Auto mode trigger distance (10-500 m) |
| **Force Join** | Action | Clear session and rejoin network |
| **Force TX** | Action | Manually trigger a transmission |
| **Show QR** | Action | Display credentials QR code |
| **< Exit** | Action | Save settings and return to main screen |

## Helium Console Setup

### 1. Create Device

1. Log in to [Helium Console](https://console.helium.com/)
2. Go to **Devices** → **Add Device**
3. Enter device credentials:
   - **Name**: Your device name
   - **Dev EUI**: From device QR code or serial output
   - **App EUI**: From device QR code or serial output
   - **App Key**: From device QR code or serial output
4. Click **Save**

### 2. Create Decoder Function

1. Go to **Functions** → **Add Function**
2. Name: "LoRa Scanner Decoder"
3. Type: **Decoder**
4. Format: **Custom Script**
5. Paste the decoder code (see [Helium Decoder](#helium-decoder) section)

### 3. Create Integration (Optional)

To forward data to your backend:

1. Go to **Integrations** → **Add Integration**
2. Choose your destination (HTTP, MQTT, AWS IoT, etc.)
3. Configure the endpoint URL and authentication

### 4. Create Label & Flow

1. Create a label for your scanner devices
2. Add the device to the label
3. Create a flow: Device → Decoder Function → Integration

## Payload Format

The device sends a 14-byte binary payload on LoRaWAN port 1:

| Bytes | Field | Type | Scale | Description |
|-------|-------|------|-------|-------------|
| 0-3 | Latitude | int32 | ×10⁷ | Signed, big-endian |
| 4-7 | Longitude | int32 | ×10⁷ | Signed, big-endian |
| 8-9 | Altitude | int16 | 1 | Meters, signed |
| 10 | HDOP | uint8 | ×10 | 0.0-25.5 |
| 11 | Satellites | uint8 | 1 | Count |
| 12 | Battery | uint8 | 1 | Percentage (0-100%) |
| 13 | TX Power | int8 | 1 | dBm |

### Example Payload

```
Hex: 19 5C 8F 10  0E 24 9A 8C  02 26  0C  08  4B  0E
     └─ Lat ──┘  └─ Lon ──┘  └Alt┘  HDOP Sat Bat TXP

Decoded:
  Latitude:  42.6977000° (0x195C8F10 = 426977000)
  Longitude: 23.7320000° (0x0E249A8C = 237320000)
  Altitude:  550m (0x0226 = 550)
  HDOP:      1.2 (12 / 10)
  Satellites: 8
  Battery:   75%
  TX Power:  14 dBm
```

## Helium Decoder

Add this JavaScript decoder in Helium Console:

```javascript
function Decoder(bytes, port) {
    var decoded = {};

    if (port === 1 && bytes.length >= 14) {
        // Latitude (bytes 0-3, signed int32, ×10^7)
        var lat = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        if (lat > 0x7FFFFFFF) lat -= 0x100000000;
        decoded.latitude = lat / 1e7;

        // Longitude (bytes 4-7, signed int32, ×10^7)
        var lon = (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7];
        if (lon > 0x7FFFFFFF) lon -= 0x100000000;
        decoded.longitude = lon / 1e7;

        // Altitude (bytes 8-9, signed int16, meters)
        var alt = (bytes[8] << 8) | bytes[9];
        if (alt > 0x7FFF) alt -= 0x10000;
        decoded.altitude = alt;

        // HDOP (byte 10, ×10)
        decoded.hdop = bytes[10] / 10;

        // Satellites (byte 11)
        decoded.satellites = bytes[11];

        // Battery (byte 12, %)
        decoded.battery = bytes[12];

        // TX Power (byte 13, signed int8, dBm)
        var txp = bytes[13];
        if (txp > 127) txp -= 256;
        decoded.txPower = txp;
    }

    return decoded;
}
```

### Alternative: Helium decodeUplink Format

```javascript
function decodeUplink(input) {
    var bytes = input.bytes;
    var port = input.fPort;
    var decoded = {};

    if (port === 1 && bytes.length >= 14) {
        var lat = (bytes[0] << 24) | (bytes[1] << 16) | (bytes[2] << 8) | bytes[3];
        if (lat > 0x7FFFFFFF) lat -= 0x100000000;
        decoded.latitude = lat / 1e7;

        var lon = (bytes[4] << 24) | (bytes[5] << 16) | (bytes[6] << 8) | bytes[7];
        if (lon > 0x7FFFFFFF) lon -= 0x100000000;
        decoded.longitude = lon / 1e7;

        var alt = (bytes[8] << 8) | bytes[9];
        if (alt > 0x7FFF) alt -= 0x10000;
        decoded.altitude = alt;

        decoded.hdop = bytes[10] / 10;
        decoded.satellites = bytes[11];
        decoded.battery = bytes[12];

        var txp = bytes[13];
        if (txp > 127) txp -= 256;
        decoded.txPower = txp;
    }

    return { data: decoded };
}
```

## Deep Sleep Mode

Deep Sleep mode provides ultra-low power operation for long-term deployments.

### Power Consumption

| State | Current | Notes |
|-------|---------|-------|
| Deep Sleep | ~10 μA | ESP32-S3 deep sleep |
| GPS Acquisition | ~30 mA | Waiting for fix |
| LoRa TX | ~120 mA | During transmission |
| Active (display on) | ~50 mA | Normal operation |

### Wake Sources

1. **Timer Wake**: Configurable 60-3600 seconds (uses Interval setting)
2. **Button Wake**: Press USER_BUTTON to exit deep sleep

### Deep Sleep Cycle

```
┌─────────────────────────────────────────────────┐
│                    DEEP SLEEP                    │
│                    (10 μA)                       │
└────────────┬────────────────────────┬───────────┘
             │                        │
        Timer Wake               Button Wake
             │                        │
             ▼                        ▼
      ┌──────────────┐         ┌─────────────┐
      │ GPS Fix Wait │         │ Exit to     │
      │ (30 mA, 2min)│         │ Manual Mode │
      └──────┬───────┘         └─────────────┘
             │
      ┌──────┴───────┐
      │ LoRaWAN Join │ (if needed)
      └──────┬───────┘
             │
      ┌──────┴───────┐
      │  TX Payload  │
      │  (120 mA)    │
      └──────┬───────┘
             │
             ▼
      Return to Deep Sleep
```

### RTC Memory

The following data is preserved across deep sleep cycles:
- `deepSleepCycles` - Count of wake cycles
- `wasInDeepSleep` - Flag for deep sleep state

## LED Patterns

The white LED (GPIO35) indicates device status:

| Pattern | Meaning |
|---------|---------|
| **Fast blink** (5 Hz) | Initializing or busy (joining/sending) |
| **Slow blink** (1 Hz) | Not joined or waiting for GPS |
| **Heartbeat** (double blink) | Ready - joined and has GPS fix |
| **Off** | Deep sleep or display timeout |

## Troubleshooting

### No Serial Output

- Heltec V3 uses **CP2102 USB-UART**, not native USB CDC
- Remove `ARDUINO_USB_CDC_ON_BOOT` flag if present
- Use 115200 baud rate
- Check COM port in Device Manager

### GPS Not Working

1. Check wiring:
   - GPS TXD → ESP32 GPIO48
   - GPS VCC → 3.3V (not 5V if possible)
   - GPS GND → GND
2. Wait outside with clear sky view (first fix can take 1-2 minutes)
3. Enable `GPS_DEBUG_OUTPUT` in config.h to see raw NMEA data
4. Check serial output for "[GPS] Stats:" messages

### Join Failures

1. Verify credentials match Helium Console exactly
2. Check MSB byte order (Helium shows MSB format)
3. Ensure device is in Helium coverage area
4. Use **Force Join** in settings menu to clear DevNonce
5. If using auto-generated credentials, register them in Helium Console first

### TX Failures (-1101 Error)

- Session expired or invalid
- Device will auto-rejoin after 3 consecutive failures
- Use **Force Join** to manually rejoin

### Battery Reading Wrong

- The ADC FET control is active HIGH (FET_ENABLE = HIGH)
- Calibration may need adjustment for your specific board
- Check `ADC_MULTIPLIER` in battery_manager.h

### Display Not Working

- Check VEXT_CTRL (GPIO36) - LOW = display on
- Verify I2C connections (SDA=17, SCL=18)
- Try power cycling the device

## Technical Specifications

### LoRaWAN

| Parameter | Value |
|-----------|-------|
| Region | EU868 |
| Activation | OTAA |
| Frequencies | 868.1, 868.3, 868.5 MHz |
| Spreading Factor | SF7-SF12 (configurable) |
| Bandwidth | 125 kHz |
| TX Power | 2-14 dBm (EU868 limit) |
| Uplink Port | 1 |

### GPS

| Parameter | Value |
|-----------|-------|
| Module | GT-U7 (u-blox compatible) |
| Baud Rate | 9600 |
| Min Satellites | 4 |
| Max HDOP | 5.0 |
| Fix Timeout | 120 seconds |

### Power

| Parameter | Value |
|-----------|-------|
| Supply Voltage | 3.3-4.2V (Li-ion) |
| USB Voltage | 5V (charging) |
| Active Current | ~50 mA |
| Deep Sleep Current | ~10 μA |
| Battery Full | 4.15V (100%) |
| Battery Empty | 3.20V (0%) |

### Display

| Parameter | Value |
|-----------|-------|
| Type | SSD1306 OLED |
| Resolution | 128×64 pixels |
| Interface | I2C (software) |
| Update Rate | 100 ms |
| Auto-off | 5 minutes |

## Project Structure

```
Heltec-LoRa32-V3_LoRaWAN-Scanner/
├── include/
│   ├── config.h              # Main configuration
│   ├── credentials.h         # LoRaWAN credentials (template)
│   └── payload_encoder.h     # Payload encoding/decoding
├── src/
│   ├── main.cpp              # Main application
│   ├── battery_manager.h     # Battery monitoring
│   ├── button_handler.h      # Button input handling
│   ├── credentials_generator.h # Credential generation
│   ├── display_manager.h     # OLED display & UI
│   ├── gps_manager.h         # GPS handling
│   ├── led_manager.h         # LED patterns
│   └── lora_manager.h        # LoRaWAN stack
├── platformio.ini            # PlatformIO configuration
└── README.md                 # This file
```

## Dependencies

Managed by PlatformIO (automatically installed):

| Library | Version | Purpose |
|---------|---------|---------|
| RadioLib | 6.6.0 | LoRaWAN stack |
| TinyGPSPlus | 1.0.3 | GPS NMEA parsing |
| U8g2 | 2.35.9 | OLED display driver |
| QRCode | 0.0.1 | QR code generation |

## License

MIT License

Copyright (c) 2024

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Acknowledgments

- [Heltec Automation](https://heltec.org/) for the LoRa32 V3 board
- [RadioLib](https://github.com/jgromes/RadioLib) for the excellent LoRaWAN library
- [Helium Network](https://www.helium.com/) for the LoRaWAN infrastructure
