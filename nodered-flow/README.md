# Node-RED Flow for LoRaWAN Signal Scanner

Node-RED function node that processes ChirpStack MQTT uplink events and formats data for InfluxDB v2.

## Overview

```
ChirpStack MQTT → MQTT In → Function Node → InfluxDB v2 Out
```

The function node extracts decoded payload fields, LoRaWAN metadata, and gateway signal quality data from ChirpStack events and formats them for InfluxDB storage.

## Setup

### 1. MQTT Input Node

- **Server**: Your ChirpStack MQTT broker (e.g., `localhost:1883`)
- **Topic**: `application/+/device/+/event/up`
- **Output**: JSON object

### 2. Function Node

1. Add a **Function** node to your flow
2. Copy the contents of `function-node.js` into the function editor
3. Set **Outputs** to 1

### 3. InfluxDB v2 Output Node

Install `node-red-contrib-influxdb` if not already installed:
```
cd ~/.node-red && npm install node-red-contrib-influxdb
```

- **Version**: 2.0
- **URL**: Your InfluxDB URL (e.g., `http://localhost:8086`)
- **Token**: Your InfluxDB API token
- **Organization**: Your org name
- **Bucket**: Your bucket name (e.g., `lora_tracker`)
- **Measurement**: `tracker`

## Output Format

The function node outputs `[fields, tags]` array for the InfluxDB v2 node.

### Fields (Values)

| Field | Type | Source | Description |
|-------|------|--------|-------------|
| `latitude` | float | Decoder | GPS latitude (degrees) |
| `longitude` | float | Decoder | GPS longitude (degrees) |
| `altitude` | int | Decoder | Altitude (meters) |
| `hdop` | float | Decoder | Horizontal dilution of precision |
| `satellites` | int | Decoder | Number of GPS satellites |
| `battery` | int | Decoder | Battery level (0-100%) |
| `txPower` | int | Decoder | TX power (dBm) |
| `fCnt` | int | LoRaWAN | Frame counter |
| `fPort` | int | LoRaWAN | Port number |
| `dr` | int | LoRaWAN | Data rate index |
| `confirmed` | int | LoRaWAN | 1 = confirmed, 0 = unconfirmed |
| `rssi` | int | Gateway | Best RSSI from all gateways (dBm) |
| `snr` | float | Gateway | SNR from best gateway (dB) |
| `gatewayCount` | int | Gateway | Number of gateways that received the packet |
| `bestGateway` | string | Gateway | Name of gateway with strongest signal |

### Tags (Indexed)

| Tag | Description |
|-----|-------------|
| `devEui` | Device EUI |
| `devID` | Friendly device name (mapped from devEui) |
| `application` | ChirpStack application name |

## Device ID Mapping

To add more devices, edit the mapping section in the function node:

```javascript
// ---- Device ID mapping ----
let devID;
if (devEui === "fcad565b79569dde") {
    devID = "HELIUM_TRACKER_1";
} else if (devEui === "your_new_deveui_here") {
    devID = "YOUR_DEVICE_NAME";
} else {
    devID = deviceName !== "unknown" ? deviceName : "UNKNOWN";
}
```

## Example InfluxDB Query (Flux)

```flux
from(bucket: "lora_tracker")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "tracker")
  |> filter(fn: (r) => r.devID == "HELIUM_TRACKER_1")
  |> filter(fn: (r) => r._field == "latitude" or r._field == "longitude" or r._field == "rssi")
  |> pivot(rowKey: ["_time"], columnKey: ["_field"], valueColumn: "_value")
```

## Grafana Integration

Use the InfluxDB data source in Grafana to visualize:

- **Map panel**: Plot GPS coordinates on a map using latitude/longitude fields
- **Gauge**: Battery level, RSSI, SNR
- **Time series**: RSSI over time, gateway count, satellite count
- **Table**: All fields per uplink event
