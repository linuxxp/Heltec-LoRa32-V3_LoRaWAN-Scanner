#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// BATTERY MANAGER CLASS
// Heltec V3 Battery Reading:
// - GPIO1 (ADC1_CH0) for voltage reading
// - GPIO37 controls FET gate (has pull-up, so LOW = enable, HIGH = disable)
// - Voltage divider: VBAT -> 390kΩ -> ADC -> 100kΩ -> GND
// - Divider ratio: (390k + 100k) / 100k = 4.9
// - ADC sees: VBAT / 4.9
// - Measurement every 30 seconds to save power
// =============================================================================

// Battery measurement interval (30 seconds as specified)
#define BATTERY_MEASURE_INTERVAL_MS  30000

// ADC calibration for ESP32-S3 with 11dB attenuation
// Reference voltage ~2600mV at 4095 counts
// Divider ratio = 4.9
// So: VBAT = (raw / 4095) * 2600 * 4.9 = raw * 3.11
#define ADC_MULTIPLIER  3.11f

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
    // Has external pull-up, controls FET gate
    // LOW = FET ON = divider connected (measuring)
    // HIGH = FET OFF = divider disconnected (power save)
    pinMode(VBAT_CTRL, OUTPUT);
    digitalWrite(VBAT_CTRL, HIGH);  // Start disabled (power save)

    // Configure ADC pin (GPIO1 = ADC1_CH0)
    pinMode(VBAT_ADC, INPUT);
    analogReadResolution(12);
    analogSetPinAttenuation(VBAT_ADC, ADC_11db);

    // Do initial measurement
    _voltage = measureVoltage();
    _percent = voltageToPercent(_voltage);
    _lastMeasure = millis();
    _initialized = true;

    DEBUG_PRINTF("[BAT] Initialized: %dmV (%d%%)\n", _voltage, _percent);
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
    // Step 1: Enable FET (LOW = connect divider to ADC)
    digitalWrite(VBAT_CTRL, LOW);

    // Step 2: Wait for voltage to stabilize
    // The RC time constant of the divider + ADC input capacitance
    delay(10);

    // Step 3: Take multiple samples and average
    uint32_t sum = 0;
    const int samples = 16;

    for (int i = 0; i < samples; i++) {
        sum += analogRead(VBAT_ADC);
        delayMicroseconds(200);
    }

    uint32_t raw = sum / samples;

    // Step 4: Disable FET (HIGH = disconnect for power save)
    digitalWrite(VBAT_CTRL, HIGH);

    // Step 5: Calculate battery voltage
    // ESP32-S3 ADC with 11dB attenuation: ~0-2600mV for 0-4095
    // Voltage divider: 390k / 100k = 4.9 ratio
    // VBAT = ADC_reading * (2600/4095) * 4.9 = raw * 3.11
    float voltage = raw * ADC_MULTIPLIER;

    // Debug output with raw value for calibration
    DEBUG_PRINTF("[BAT] RAW=%lu, Voltage=%.0fmV\n", raw, voltage);

    // Sanity check
    if (raw < 100) {
        DEBUG_PRINTLN("[BAT] WARNING: ADC reading very low - check connections!");
        DEBUG_PRINTF("[BAT] VBAT_ADC=%d, VBAT_CTRL=%d\n", VBAT_ADC, VBAT_CTRL);
    }

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
