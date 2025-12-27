#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// BATTERY MANAGER CLASS
// Heltec V3 Battery Reading:
// - GPIO1 (ADC1_CH0) for voltage reading
// - GPIO37 controls FET gate (active HIGH - HIGH = enable, LOW = disable)
// - Voltage divider: VBAT -> 390kΩ -> ADC -> 100kΩ -> GND
// - Divider ratio: (390k + 100k) / 100k = 4.9
// - ADC sees: VBAT / 4.9
// - Measurement every 30 seconds to save power
// =============================================================================

// Battery measurement interval (30 seconds as specified)
#define BATTERY_MEASURE_INTERVAL_MS  30000

// FET control logic: HIGH = enable (connect divider), LOW = disable
// (Confirmed by runtime test - opposite of what datasheet suggests)
#define FET_ENABLE   HIGH
#define FET_DISABLE  LOW

// ADC calibration for ESP32-S3 with 11dB attenuation
// Calibrated: measured 4184mV, showed 2935mV, so multiplier = 4184/944 = 4.43
#define ADC_MULTIPLIER  4.43f

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
    uint32_t _lastMeasure = 0;
    bool _initialized = false;

    uint16_t measureVoltage();
    uint8_t voltageToPercent(uint16_t voltage);
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void BatteryManager::begin() {
    // Configure ADC control pin (GPIO37)
    pinMode(VBAT_CTRL, OUTPUT);
    digitalWrite(VBAT_CTRL, FET_DISABLE);  // Start disabled (power save)

    // Configure ADC pin (GPIO1 = ADC1_CH0)
    pinMode(VBAT_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(VBAT_ADC, ADC_11db);

    // Do initial measurement
    _voltage = measureVoltage();
    _percent = voltageToPercent(_voltage);
    _lastMeasure = millis();
    _initialized = true;

    DEBUG_PRINTF("[BAT] %dmV (%d%%)\n", _voltage, _percent);
}

inline void BatteryManager::update() {
    if (!_initialized) return;

    uint32_t now = millis();

    // Only measure every 30 seconds
    if ((now - _lastMeasure) < BATTERY_MEASURE_INTERVAL_MS) {
        return;
    }
    _lastMeasure = now;

    _voltage = measureVoltage();
    _percent = voltageToPercent(_voltage);

    // Charging detection: voltage above 4.2V usually means charging
    _charging = (_voltage > BATTERY_FULL_MV);

    DEBUG_PRINTF("[BAT] Update: %dmV (%d%%)%s\n",
        _voltage, _percent, _charging ? " [CHARGING]" : "");
}

inline uint16_t BatteryManager::measureVoltage() {
    // Enable FET to connect divider to ADC
    digitalWrite(VBAT_CTRL, FET_ENABLE);
    delay(20);  // Wait for voltage to stabilize

    // Take multiple samples and average
    uint32_t sum = 0;
    const int samples = 16;

    for (int i = 0; i < samples; i++) {
        sum += analogRead(VBAT_ADC);
        delayMicroseconds(500);
    }

    uint32_t raw = sum / samples;

    // Disable FET for power save
    digitalWrite(VBAT_CTRL, FET_DISABLE);

    // Calculate battery voltage using calibrated multiplier
    float voltage = raw * ADC_MULTIPLIER;

    return (uint16_t)voltage;
}

inline uint8_t BatteryManager::voltageToPercent(uint16_t voltage) {
    if (voltage >= BATTERY_FULL_MV) return 100;
    if (voltage <= BATTERY_EMPTY_MV) return 0;

    // Piecewise linear approximation of LiPo discharge curve
    // 4200mV = 100%, 4100mV = 90%, 3900mV = 60%, 3700mV = 30%, 3500mV = 10%, 3200mV = 0%
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
