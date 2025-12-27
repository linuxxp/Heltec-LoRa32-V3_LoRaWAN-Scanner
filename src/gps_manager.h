#ifndef GPS_MANAGER_H
#define GPS_MANAGER_H

#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"
#include "payload_encoder.h"

// =============================================================================
// GPS MANAGER CLASS
// =============================================================================
class GPSManager {
public:
    void begin(HardwareSerial& serial, uint8_t rxPin = GPS_RX_PIN, uint8_t txPin = GPS_TX_PIN);
    void update();
    void powerOn();
    void powerOff();

    // Data access
    GPSData getData() const { return _data; }
    bool hasValidFix() const { return _data.valid; }
    bool isUpdated() const { return _updated; }
    void clearUpdated() { _updated = false; }

    // Distance calculation (for auto mode)
    float distanceFrom(double lat, double lon) const;
    void savePosition();  // Save current position as reference
    float distanceFromSaved() const;

    // Statistics
    uint32_t getFixAge() const;
    uint32_t getTimeSinceFix() const { return millis() - _lastFixTime; }
    uint32_t getCharsProcessed() const { return _gps.charsProcessed(); }
    uint32_t getSentencesWithFix() const { return _gps.sentencesWithFix(); }
    uint32_t getFailedChecksums() const { return _gps.failedChecksum(); }

private:
    TinyGPSPlus _gps;
    HardwareSerial* _serial = nullptr;
    GPSData _data;

    bool _updated = false;
    bool _powerOn = false;
    uint32_t _lastFixTime = 0;
    uint32_t _lastStatsTime = 0;

    // Saved position for distance calculation
    double _savedLat = 0;
    double _savedLon = 0;
    bool _hasSavedPosition = false;

    void updateData();
    bool validateFix();
    void printStats();
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void GPSManager::begin(HardwareSerial& serial, uint8_t rxPin, uint8_t txPin) {
    _serial = &serial;

    // Setup GPS EN pin for power control (if used)
    // Note: GT-U7 is typically always on when powered via VCC
    pinMode(GPS_EN_PIN, OUTPUT);
    powerOn();

    // Initialize serial
    _serial->begin(GPS_BAUD, SERIAL_8N1, rxPin, txPin);

    // Initialize data structure
    _data = {0, 0, 0, 0, 0, false};

    DEBUG_PRINTF("[GPS] Initialized on RX:%d TX:%d EN:%d @ %d baud\n", rxPin, txPin, GPS_EN_PIN, GPS_BAUD);
    DEBUG_PRINTLN("[GPS] Waiting for GPS data...");

    #if GPS_DEBUG_OUTPUT
    DEBUG_PRINTLN("[GPS] GPS_DEBUG_OUTPUT is ENABLED - raw NMEA will be printed");
    #endif
}

inline void GPSManager::update() {
    if (!_serial || !_powerOn) return;

    // Read all available data
    while (_serial->available() > 0) {
        char c = _serial->read();

        // Debug: print raw GPS data to serial
        #if GPS_DEBUG_OUTPUT
        DEBUG_SERIAL.write(c);
        #endif

        if (_gps.encode(c)) {
            updateData();
        }
    }

    // Print GPS stats periodically (every 30 seconds)
    uint32_t now = millis();
    if ((now - _lastStatsTime) >= 30000) {
        _lastStatsTime = now;
        printStats();
    }
}

inline void GPSManager::printStats() {
    DEBUG_PRINTF("[GPS] Stats: chars=%lu, sentences=%lu, failed=%lu, sats=%d, fix=%s\n",
        _gps.charsProcessed(),
        _gps.sentencesWithFix(),
        _gps.failedChecksum(),
        _gps.satellites.value(),
        _data.valid ? "YES" : "NO");

    if (_gps.charsProcessed() == 0) {
        DEBUG_PRINTLN("[GPS] WARNING: No data received! Check wiring:");
        DEBUG_PRINTLN("[GPS]   - GPS TXD -> ESP32 GPIO48 (RX)");
        DEBUG_PRINTLN("[GPS]   - GPS RXD -> ESP32 GPIO47 (TX)");
        DEBUG_PRINTLN("[GPS]   - GPS VCC -> 3.3V or 5V");
        DEBUG_PRINTLN("[GPS]   - GPS GND -> GND");
    }
}

inline void GPSManager::powerOn() {
    digitalWrite(GPS_EN_PIN, HIGH);  // EN HIGH = GPS on
    _powerOn = true;
    delay(100);  // Let GPS stabilize
    DEBUG_PRINTLN("[GPS] Power ON");
}

inline void GPSManager::powerOff() {
    digitalWrite(GPS_EN_PIN, LOW);  // EN LOW = GPS off
    _powerOn = false;
    DEBUG_PRINTLN("[GPS] Power OFF");
}

inline void GPSManager::updateData() {
    if (_gps.location.isUpdated()) {
        _data.latitude = _gps.location.lat();
        _data.longitude = _gps.location.lng();
    }

    if (_gps.altitude.isUpdated()) {
        _data.altitude = _gps.altitude.meters();
    }

    if (_gps.hdop.isUpdated()) {
        _data.hdop = _gps.hdop.hdop();
    }

    if (_gps.satellites.isUpdated()) {
        _data.satellites = _gps.satellites.value();
    }

    // Check if we have a valid fix
    bool wasValid = _data.valid;
    _data.valid = validateFix();

    if (_data.valid) {
        _lastFixTime = millis();
        _updated = true;

        if (!wasValid) {
            DEBUG_PRINTLN("[GPS] Fix acquired!");
            DEBUG_PRINTF("[GPS] Position: %.6f, %.6f\n", _data.latitude, _data.longitude);
        }
    }
}

inline bool GPSManager::validateFix() {
    // Check if location is valid and recent
    if (!_gps.location.isValid()) return false;
    if (_gps.location.age() > 2000) return false;  // Max 2 seconds old

    // Check minimum satellites
    if (_gps.satellites.value() < GPS_MIN_SATELLITES) return false;

    // Check HDOP
    if (_gps.hdop.hdop() > GPS_MAX_HDOP) return false;

    // Sanity check on coordinates
    if (_data.latitude == 0 && _data.longitude == 0) return false;
    if (abs(_data.latitude) > 90) return false;
    if (abs(_data.longitude) > 180) return false;

    return true;
}

inline float GPSManager::distanceFrom(double lat, double lon) const {
    if (!_data.valid) return -1;

    return TinyGPSPlus::distanceBetween(
        _data.latitude, _data.longitude,
        lat, lon
    );
}

inline void GPSManager::savePosition() {
    if (_data.valid) {
        _savedLat = _data.latitude;
        _savedLon = _data.longitude;
        _hasSavedPosition = true;
        DEBUG_PRINTF("[GPS] Position saved: %.6f, %.6f\n", _savedLat, _savedLon);
    }
}

inline float GPSManager::distanceFromSaved() const {
    if (!_hasSavedPosition || !_data.valid) return -1;
    return distanceFrom(_savedLat, _savedLon);
}

inline uint32_t GPSManager::getFixAge() const {
    return _gps.location.age();
}

#endif // GPS_MANAGER_H
