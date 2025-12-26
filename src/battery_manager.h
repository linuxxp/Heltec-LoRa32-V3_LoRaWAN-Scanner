#ifndef BATTERY_MANAGER_H
#define BATTERY_MANAGER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// BATTERY MANAGER CLASS
// Heltec V3 Battery Reading:
// - GPIO1 (ADC1_CH0) for voltage reading
// - GPIO37 controls voltage divider (LOW = enable, HIGH = disable)
// - Voltage divider: VBAT -> 390kΩ -> GPIO1 -> 100kΩ -> GND
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
    // Heltec V3: GPIO37 controls the voltage divider
    // LOW = enable reading, HIGH = disable (save power)
    pinMode(VBAT_CTRL, OUTPUT);
    digitalWrite(VBAT_CTRL, LOW);  // Enable for initial reading

    // Configure ADC for battery pin (GPIO1 = ADC1_CH0)
    // Use ADC1 which is more reliable on ESP32-S3
    analogReadResolution(12);  // 12-bit resolution (0-4095)
    analogSetAttenuation(ADC_11db);  // Full range ~0-3.3V

    delay(50);  // Let ADC stabilize

    // Initial reading
    update();

    // Disable voltage divider to save power
    digitalWrite(VBAT_CTRL, HIGH);

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
        delayMicroseconds(500);
    }
    uint32_t raw = sum / BATTERY_ADC_SAMPLES;

    // Disable voltage divider to save power
    digitalWrite(VBAT_CTRL, HIGH);

    // Heltec V3 voltage calculation:
    // ADC: 12-bit (0-4095), with ADC_11db attenuation reads ~0-2.5V
    // Voltage divider ratio: (390k + 100k) / 100k = 4.9
    //
    // Based on Meshtastic firmware and ropg's heltec_esp32_lora_v3 library:
    // The actual voltage = ADC_reading * (reference_voltage / 4095) * divider_ratio
    // With calibration factor for Heltec V3:
    // Vbat = raw * 0.00159 * 1000 (in mV) or approximately raw * 1.6
    //
    // Alternative formula from Heltec: raw * XS * MUL where XS=0.0025, MUL=1000
    // = raw * 2.5
    //
    // Using empirically calibrated value for Heltec V3:
    float voltage = raw * 1.6f;

    // Sanity check - if reading seems wrong, try alternative calculation
    if (voltage < 2500 || voltage > 5000) {
        // Alternative: use raw * 2.5 from Heltec official example
        voltage = raw * 2.5f;
    }

    DEBUG_PRINTF("[BAT] Raw: %lu, Voltage: %.0fmV\n", raw, voltage);

    return (uint16_t)voltage;
}

inline uint8_t BatteryManager::voltageToPercent(uint16_t voltage) {
    // Li-ion/LiPo discharge curve approximation
    // Based on typical LiPo discharge characteristics

    if (voltage >= BATTERY_FULL_MV) return 100;
    if (voltage <= BATTERY_EMPTY_MV) return 0;

    // Piecewise linear approximation matching real LiPo curve
    // From ropg's heltec_esp32_lora_v3 library calibration
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
