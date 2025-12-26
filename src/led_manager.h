#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <Arduino.h>

// =============================================================================
// LED MANAGER CLASS
// Heltec V3 has a white LED on GPIO35
// =============================================================================
class LEDManager {
public:
    void begin(uint8_t pin = LED_PIN);
    void on();
    void off();
    void toggle();
    void blink(uint16_t onMs = 100, uint16_t offMs = 100, uint8_t count = 1);
    void update();  // Call in loop for async blinking

    // Status patterns
    void setPattern(uint8_t pattern);
    static const uint8_t PATTERN_OFF = 0;
    static const uint8_t PATTERN_ON = 1;
    static const uint8_t PATTERN_SLOW_BLINK = 2;   // 1Hz
    static const uint8_t PATTERN_FAST_BLINK = 3;   // 5Hz
    static const uint8_t PATTERN_HEARTBEAT = 4;    // Double blink

private:
    uint8_t _pin = LED_PIN;
    bool _state = false;
    uint8_t _pattern = PATTERN_OFF;
    uint32_t _lastUpdate = 0;
    uint8_t _blinkPhase = 0;
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void LEDManager::begin(uint8_t pin) {
    _pin = pin;
    pinMode(_pin, OUTPUT);
    off();
    DEBUG_PRINTF("[LED] Initialized on GPIO%d\n", _pin);
}

inline void LEDManager::on() {
    digitalWrite(_pin, HIGH);
    _state = true;
}

inline void LEDManager::off() {
    digitalWrite(_pin, LOW);
    _state = false;
}

inline void LEDManager::toggle() {
    _state = !_state;
    digitalWrite(_pin, _state ? HIGH : LOW);
}

inline void LEDManager::blink(uint16_t onMs, uint16_t offMs, uint8_t count) {
    for (uint8_t i = 0; i < count; i++) {
        on();
        delay(onMs);
        off();
        if (i < count - 1) {
            delay(offMs);
        }
    }
}

inline void LEDManager::setPattern(uint8_t pattern) {
    _pattern = pattern;
    _blinkPhase = 0;
    if (pattern == PATTERN_OFF) {
        off();
    } else if (pattern == PATTERN_ON) {
        on();
    }
}

inline void LEDManager::update() {
    uint32_t now = millis();
    uint32_t elapsed = now - _lastUpdate;

    switch (_pattern) {
        case PATTERN_OFF:
        case PATTERN_ON:
            // Static state, nothing to do
            break;

        case PATTERN_SLOW_BLINK:
            // 500ms on, 500ms off
            if (elapsed >= 500) {
                toggle();
                _lastUpdate = now;
            }
            break;

        case PATTERN_FAST_BLINK:
            // 100ms on, 100ms off
            if (elapsed >= 100) {
                toggle();
                _lastUpdate = now;
            }
            break;

        case PATTERN_HEARTBEAT:
            // Double blink pattern: on-off-on-off-pause
            // Phase 0: on (100ms)
            // Phase 1: off (100ms)
            // Phase 2: on (100ms)
            // Phase 3: off (700ms)
            switch (_blinkPhase) {
                case 0:
                    if (elapsed >= 100) {
                        off();
                        _blinkPhase = 1;
                        _lastUpdate = now;
                    }
                    break;
                case 1:
                    if (elapsed >= 100) {
                        on();
                        _blinkPhase = 2;
                        _lastUpdate = now;
                    }
                    break;
                case 2:
                    if (elapsed >= 100) {
                        off();
                        _blinkPhase = 3;
                        _lastUpdate = now;
                    }
                    break;
                case 3:
                    if (elapsed >= 700) {
                        on();
                        _blinkPhase = 0;
                        _lastUpdate = now;
                    }
                    break;
            }
            break;
    }
}

#endif // LED_MANAGER_H
