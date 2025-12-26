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
    // Enable battery voltage reading on Heltec V3
    pinMode(VBAT_CTRL, OUTPUT);
    digitalWrite(VBAT_CTRL, LOW);  // Enable voltage divider

    // Configure ADC
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

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
    // Enable voltage divider
    digitalWrite(VBAT_CTRL, LOW);
    delay(10);  // Let it stabilize

    // Take multiple samples and average
    uint32_t sum = 0;
    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
        sum += analogRead(VBAT_ADC);
        delayMicroseconds(100);
    }
    uint32_t raw = sum / BATTERY_ADC_SAMPLES;

    // Disable voltage divider to save power
    digitalWrite(VBAT_CTRL, HIGH);

    // Heltec V3 uses a voltage divider: 390k / 100k
    // ADC reference is 3.3V with 12-bit resolution
    // Vbat = Vadc * (390 + 100) / 100 = Vadc * 4.9
    // Vadc = raw * 3300 / 4095
    // Vbat = raw * 3300 * 4.9 / 4095 ≈ raw * 3.95

    // Calibration factor (adjust if readings are off)
    const float calibration = 3.95f;
    uint16_t voltage = (uint16_t)(raw * calibration);

    return voltage;
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
