#ifndef PAYLOAD_ENCODER_H
#define PAYLOAD_ENCODER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// PAYLOAD STRUCTURE (14 bytes total)
// =============================================================================
// Byte 0-3:   Latitude (int32, ×10^7)     - allows ±214.7483647 degrees
// Byte 4-7:   Longitude (int32, ×10^7)    - allows ±214.7483647 degrees
// Byte 8-9:   Altitude (int16, meters)    - allows -32768 to +32767 m
// Byte 10:    HDOP (uint8, ×10)           - allows 0.0 to 25.5
// Byte 11:    Satellites (uint8)          - allows 0 to 255
// Byte 12:    Battery (uint8, %)          - allows 0 to 100%
// Byte 13:    TX Power (int8, dBm)        - allows -128 to +127 dBm

struct GPSData {
    double latitude;
    double longitude;
    float altitude;
    float hdop;
    uint8_t satellites;
    bool valid;
};

struct ScannerPayload {
    int32_t latitude;       // ×10^7
    int32_t longitude;      // ×10^7
    int16_t altitude;       // meters
    uint8_t hdop;           // ×10
    uint8_t satellites;
    uint8_t battery;        // %
    int8_t txPower;         // dBm
};

class PayloadEncoder {
public:
    // Encode GPS data + device status into binary payload
    static uint8_t encode(
        const GPSData& gps,
        uint8_t batteryPercent,
        int8_t txPower,
        uint8_t* buffer,
        size_t bufferSize
    ) {
        if (bufferSize < PAYLOAD_SIZE) {
            return 0;
        }

        ScannerPayload payload;

        // Convert GPS coordinates to fixed-point
        payload.latitude = (int32_t)(gps.latitude * 1e7);
        payload.longitude = (int32_t)(gps.longitude * 1e7);
        payload.altitude = (int16_t)constrain(gps.altitude, -32768, 32767);
        payload.hdop = (uint8_t)constrain(gps.hdop * 10, 0, 255);
        payload.satellites = gps.satellites;
        payload.battery = batteryPercent;
        payload.txPower = txPower;

        // Pack into buffer (big-endian for network transmission)
        buffer[0] = (payload.latitude >> 24) & 0xFF;
        buffer[1] = (payload.latitude >> 16) & 0xFF;
        buffer[2] = (payload.latitude >> 8) & 0xFF;
        buffer[3] = payload.latitude & 0xFF;

        buffer[4] = (payload.longitude >> 24) & 0xFF;
        buffer[5] = (payload.longitude >> 16) & 0xFF;
        buffer[6] = (payload.longitude >> 8) & 0xFF;
        buffer[7] = payload.longitude & 0xFF;

        buffer[8] = (payload.altitude >> 8) & 0xFF;
        buffer[9] = payload.altitude & 0xFF;

        buffer[10] = payload.hdop;
        buffer[11] = payload.satellites;
        buffer[12] = payload.battery;
        buffer[13] = (uint8_t)payload.txPower;

        return PAYLOAD_SIZE;
    }

    // Decode binary payload back to struct (for testing)
    static bool decode(const uint8_t* buffer, size_t len, ScannerPayload& payload) {
        if (len < PAYLOAD_SIZE) {
            return false;
        }

        payload.latitude = ((int32_t)buffer[0] << 24) |
                          ((int32_t)buffer[1] << 16) |
                          ((int32_t)buffer[2] << 8) |
                          (int32_t)buffer[3];

        payload.longitude = ((int32_t)buffer[4] << 24) |
                           ((int32_t)buffer[5] << 16) |
                           ((int32_t)buffer[6] << 8) |
                           (int32_t)buffer[7];

        payload.altitude = ((int16_t)buffer[8] << 8) | buffer[9];
        payload.hdop = buffer[10];
        payload.satellites = buffer[11];
        payload.battery = buffer[12];
        payload.txPower = (int8_t)buffer[13];

        return true;
    }

    // Get latitude as double from payload
    static double getLatitude(const ScannerPayload& payload) {
        return payload.latitude / 1e7;
    }

    // Get longitude as double from payload
    static double getLongitude(const ScannerPayload& payload) {
        return payload.longitude / 1e7;
    }

    // Get HDOP as float from payload
    static float getHdop(const ScannerPayload& payload) {
        return payload.hdop / 10.0f;
    }

    // Print payload for debugging
    static void printPayload(const uint8_t* buffer, size_t len) {
        DEBUG_PRINT("Payload (");
        DEBUG_PRINT(len);
        DEBUG_PRINT(" bytes): ");
        for (size_t i = 0; i < len; i++) {
            if (buffer[i] < 0x10) DEBUG_PRINT("0");
            DEBUG_PRINT(String(buffer[i], HEX));
            DEBUG_PRINT(" ");
        }
        DEBUG_PRINTLN("");
    }
};

// =============================================================================
// HELIUM CONSOLE DECODER (JavaScript)
// =============================================================================
/*
Copy this decoder function to your Helium Console:

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
*/

#endif // PAYLOAD_ENCODER_H
