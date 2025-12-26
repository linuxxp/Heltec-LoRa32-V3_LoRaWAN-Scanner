#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include <esp_adc_cal.h>
#include "config.h"

// =============================================================================
// BATTERY MANAGER CLASS
// Heltec V3 Battery Reading:
// - GPIO1 (ADC1_CH0) for voltage reading
// - GPIO37 controls FET gate (has pull-up, so LOW = enable reading)
// - Voltage divider: VBAT -> 390kΩ -> ADC -> 100kΩ -> GND
// - Divider ratio: (390k + 100k) / 100k = 4.9
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
    // Configure ADC control pin
    // GPIO37 has external pull-up, controls FET gate
    // LOW = FET ON = battery voltage divider connected to ADC
    // HIGH = FET OFF = disconnected (default state due to pull-up)
    pinMode(VBAT_CTRL, OUTPUT);

    // Configure ADC pin
    pinMode(VBAT_ADC, INPUT);

    // Configure ADC with proper attenuation for the specific pin
    analogReadResolution(12);
    analogSetPinAttenuation(VBAT_ADC, ADC_11db);

    // Enable reading and wait for stabilization
    digitalWrite(VBAT_CTRL, LOW);
    delay(100);  // Longer delay for initial stabilization

    // Force first reading
    _lastUpdate = 0;
    update();

    // Keep enabled for continuous monitoring (can disable to save power)
    // digitalWrite(VBAT_CTRL, HIGH);

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

    // Charging detection: voltage above 4.2V usually means charging
    _charging = (_voltage > BATTERY_FULL_MV);
}

inline uint16_t BatteryManager::readVoltage() {
    // Ensure FET is enabled (LOW = enable)
    digitalWrite(VBAT_CTRL, LOW);
    delay(5);  // Short stabilization delay

    // Take multiple samples and average
    uint32_t sum = 0;
    int validSamples = 0;

    for (int i = 0; i < BATTERY_ADC_SAMPLES; i++) {
        uint16_t sample = analogRead(VBAT_ADC);
        sum += sample;
        validSamples++;
        delayMicroseconds(100);
    }

    uint32_t raw = (validSamples > 0) ? (sum / validSamples) : 0;

    // Heltec V3 voltage calculation:
    // ESP32-S3 ADC with 11db attenuation: ~0-3100mV range for 0-4095
    // Voltage divider ratio: 4.9 (390k + 100k) / 100k
    //
    // ADC voltage = raw * (3100 / 4095) = raw * 0.757
    // Battery voltage = ADC voltage * 4.9 = raw * 3.71
    //
    // But ESP32-S3 ADC is not perfectly linear, so we use calibrated value
    // Based on Heltec examples: multiplier around 3.2 - 3.7

    float voltage = raw * 3.7f;  // Calibrated multiplier

    // Debug output
    DEBUG_PRINTF("[BAT] ADC raw=%lu, calculated=%0.fmV\n", raw, voltage);

    // Sanity check
    if (voltage < 1000) {
        DEBUG_PRINTLN("[BAT] WARNING: Very low reading, check battery connection");
    }

    return (uint16_t)voltage;
}

inline uint8_t BatteryManager::voltageToPercent(uint16_t voltage) {
    if (voltage >= BATTERY_FULL_MV) return 100;
    if (voltage <= BATTERY_EMPTY_MV) return 0;

    // Piecewise linear approximation of LiPo discharge curve
    if (voltage >= 4100) {
        return 90 + (voltage - 4100) * 10 / 100;
    } else if (voltage >= 3900) {
        return 60 + (voltage - 3900) * 30 / 200;
    } else if (voltage >= 3700) {
        return 30 + (voltage - 3700) * 30 / 200;
    } else if (voltage >= 3500) {
        return 10 + (voltage - 3500) * 20 / 200;
    } else {
        return (voltage - BATTERY_EMPTY_MV) * 10 / 300;
    }
}

#endif // BATTERY_MANAGER_H
