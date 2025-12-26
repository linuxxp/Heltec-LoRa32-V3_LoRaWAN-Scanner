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

// FET control logic (try HIGH to enable if LOW doesn't work)
#define FET_ENABLE   HIGH
#define FET_DISABLE  LOW

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
    DEBUG_PRINTLN("[BAT] Initializing battery manager...");
    DEBUG_PRINTF("[BAT] VBAT_ADC=GPIO%d, VBAT_CTRL=GPIO%d\n", VBAT_ADC, VBAT_CTRL);
    DEBUG_PRINTF("[BAT] FET_ENABLE=%s, FET_DISABLE=%s\n",
        FET_ENABLE == HIGH ? "HIGH" : "LOW",
        FET_DISABLE == HIGH ? "HIGH" : "LOW");

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
    // Step 1: Enable FET to connect divider to ADC
    digitalWrite(VBAT_CTRL, FET_ENABLE);

    // Step 2: Wait for voltage to stabilize
    delay(20);

    // Step 3: Take multiple samples and average
    uint32_t sum = 0;
    const int samples = 16;

    for (int i = 0; i < samples; i++) {
        sum += analogRead(VBAT_ADC);
        delayMicroseconds(500);
    }

    uint32_t raw = sum / samples;

    // Step 4: Disable FET for power save
    digitalWrite(VBAT_CTRL, FET_DISABLE);

    // Step 5: Calculate battery voltage
    // ESP32-S3 ADC with 11dB attenuation: ~0-2600mV for 0-4095
    // Voltage divider: 390k / 100k = 4.9 ratio
    // VBAT = ADC_reading * (2600/4095) * 4.9 = raw * 3.11
    float voltage = raw * ADC_MULTIPLIER;

    // Debug output with raw value for calibration
    DEBUG_PRINTF("[BAT] RAW=%lu (FET=%s), Voltage=%.0fmV\n",
        raw, FET_ENABLE == HIGH ? "HIGH" : "LOW", voltage);

    // If still getting 0, try reading with opposite FET state for debug
    if (raw < 50) {
        DEBUG_PRINTLN("[BAT] Low reading - testing opposite FET logic...");
        digitalWrite(VBAT_CTRL, FET_ENABLE == HIGH ? LOW : HIGH);
        delay(20);
        uint32_t testRaw = 0;
        for (int i = 0; i < 8; i++) {
            testRaw += analogRead(VBAT_ADC);
            delayMicroseconds(500);
        }
        testRaw /= 8;
        DEBUG_PRINTF("[BAT] With opposite FET: RAW=%lu\n", testRaw);
        digitalWrite(VBAT_CTRL, FET_DISABLE);  // Return to disabled

        // If opposite works better, use that value
        if (testRaw > raw * 2 && testRaw > 100) {
            DEBUG_PRINTLN("[BAT] NOTE: Opposite FET logic works! Update FET_ENABLE/FET_DISABLE!");
            raw = testRaw;
            voltage = raw * ADC_MULTIPLIER;
        }
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
