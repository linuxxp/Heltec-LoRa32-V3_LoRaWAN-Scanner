# ChirpStack Payload Decoder

JavaScript decoder for the LoRaWAN Signal Scanner payload. Used in ChirpStack to decode the raw binary uplink into readable JSON fields.

## Installation

1. Open **ChirpStack** web interface
2. Go to **Device Profiles** → select your profile (or create a new one)
3. Click the **Codec** tab
4. Set **Codec** to **JavaScript**
5. Paste the contents of `decoder.js` into the **Decode** editor
6. Click **Submit**

## Payload Format

The device sends a **14-byte** binary payload on **fPort 1**:

| Bytes | Field | Type | Scale | Range |
|-------|-------|------|-------|-------|
| 0-3 | Latitude | int32 | ×10⁷ | ±90° |
| 4-7 | Longitude | int32 | ×10⁷ | ±180° |
| 8-9 | Altitude | int16 | 1 | -32768 to +32767 m |
| 10 | HDOP | uint8 | ×10 | 0.0-25.5 |
| 11 | Satellites | uint8 | 1 | 0-255 |
| 12 | Battery | uint8 | 1 | 0-100% |
| 13 | TX Power | int8 | 1 | -128 to +127 dBm |

All multi-byte fields are **big-endian** (MSB first).

## Decoded Output Example

Input (Base64): `GWZZNw30fwECPAsGZA4=`

```json
{
    "latitude": 42.6269015,
    "longitude": 23.3504513,
    "altitude": 572,
    "hdop": 1.1,
    "satellites": 6,
    "battery": 100,
    "txPower": 14
}
```

## Testing

In the ChirpStack **Device** → **Events** tab, you can see decoded payloads in real-time. Verify that latitude/longitude values match expected GPS coordinates.
