#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// BATTERY MANAGER CLASS
// =============================================================================
class BatteryManager {
public:
    void begin();
    void update();

    uint16_t getVoltage() const { return _voltage; }       // mV
    uint8_t getPercent() const { return _percent; }        // 0-100
    bool isCharging() const { return _charging; }
    bool isLow() const { return _percent < 20; }
    bool isCritical() const { return _percent < 10; }

private:
    uint16_t _voltage = 0;
    uint8_t _percent = 0;
    bool _charging = false;
    uint32_t _lastUpdate = 0;

    uint16_t readVoltage();
    uint8_t voltageToPercent(uint16_t voltage);
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void BatteryManager::begin() {
    // Setup ADC control pin (Heltec V3: LOW = enable reading)
    pinMode(VBAT_CTRL, OUTPUT);
    digitalWrite(VBAT_CTRL, HIGH);  // Start disabled

    // Configure ADC for ESP32-S3
    analogReadResolution(12);
    analogSetPinAttenuation(VBAT_ADC, ADC_11db);

    // Initial reading
    update();

    DEBUG_PRINTF("[BAT] Initialized: %dmV (%d%%)\n", _voltage, _percent);
}

inline void BatteryManager::update() {
    uint32_t now = millis();

    if ((now - _lastUpdate) < BATTERY_UPDATE_MS && _lastUpdate > 0) {
        return;
    }
    _lastUpdate = now;

    _voltage = readVoltage();
    _percent = voltageToPercent(_voltage);

    // Simple charging detection: voltage above 4.2V usually means charging
    // Note: This is a rough estimation, proper charging detection needs more hardware
    _charging = (_voltage > BATTERY_FULL_MV);
}

inline uint16_t BatteryManager::readVoltage() {
    // Enable voltage divider (LOW = enabled on Heltec V3)
    digitalWrite(VBAT_CTRL, LOW);
    delay(20);  // Let it stabilize

    // Take multiple samples and average
    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
        sum += analogRead(VBAT_ADC);
        delay(2);
    }
    uint32_t raw = sum / BATTERY_ADC_SAMPLES;

    // Disable voltage divider to save power
    digitalWrite(VBAT_CTRL, HIGH);

    // Heltec V3 voltage calculation:
    // ADC: 12-bit (0-4095), reference ~2.6V with ADC_11db attenuation
    // Voltage divider: 390k / 100k = ratio 4.9
    // Formula from Heltec: Vbat = raw * (1/4096) * 3.3 * 4.01 * 1000
    // Simplified: Vbat(mV) = raw * 3.23

    float voltage = raw * 3.23f;

    DEBUG_PRINTF("[BAT] Raw ADC: %lu, Voltage: %.0fmV\n", raw, voltage);

    return (uint16_t)voltage;
}

inline uint8_t BatteryManager::voltageToPercent(uint16_t voltage) {
    // Li-ion discharge curve approximation
    // More accurate than linear mapping

    if (voltage >= BATTERY_FULL_MV) return 100;
    if (voltage <= BATTERY_EMPTY_MV) return 0;

    // Piecewise linear approximation of Li-ion discharge curve
    // Based on typical 18650 cell characteristics
    if (voltage >= 4100) {
        // 4.2V - 4.1V: 100% - 90%
        return 90 + (voltage - 4100) * 10 / 100;
    } else if (voltage >= 3900) {
        // 4.1V - 3.9V: 90% - 60%
        return 60 + (voltage - 3900) * 30 / 200;
    } else if (voltage >= 3700) {
        // 3.9V - 3.7V: 60% - 30%
        return 30 + (voltage - 3700) * 30 / 200;
    } else if (voltage >= 3500) {
        // 3.7V - 3.5V: 30% - 10%
        return 10 + (voltage - 3500) * 20 / 200;
    } else {
        // 3.5V - 3.2V: 10% - 0%
        return (voltage - BATTERY_EMPTY_MV) * 10 / 300;
    }
}

#endif // BATTERY_MANAGER_H
