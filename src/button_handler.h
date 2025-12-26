#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// BUTTON EVENTS
// =============================================================================
enum class ButtonEvent : uint8_t {
    NONE            = 0,
    SINGLE_CLICK    = 1,
    DOUBLE_CLICK    = 2,
    LONG_PRESS      = 3,
    VERY_LONG_PRESS = 4
};

// =============================================================================
// INTERRUPT-BASED BUTTON HANDLER
// Uses hardware interrupts to capture button state changes even during
// blocking operations (like software I2C display updates)
// =============================================================================
class ButtonHandler {
public:
    void begin(uint8_t pin = USER_BUTTON);
    void update();
    ButtonEvent getEvent();
    bool isPressed();

    // Callback support
    using EventCallback = void (*)(ButtonEvent);
    void setCallback(EventCallback cb) { _callback = cb; }

    // ISR needs access to these
    static void IRAM_ATTR buttonISR();
    static ButtonHandler* _instance;

private:
    uint8_t _pin;

    // State machine
    enum class State {
        IDLE,
        PRESSED,
        WAIT_DOUBLE,
        LONG_PRESSING
    };

    State _state = State::IDLE;
    ButtonEvent _pendingEvent = ButtonEvent::NONE;

    uint32_t _pressTime = 0;
    uint32_t _releaseTime = 0;
    uint8_t _clickCount = 0;

    EventCallback _callback = nullptr;

    // Interrupt-captured state (volatile for ISR safety)
    volatile bool _isrPressed = false;
    volatile uint32_t _isrPressTime = 0;
    volatile uint32_t _isrReleaseTime = 0;
    volatile bool _isrPressEvent = false;
    volatile bool _isrReleaseEvent = false;

    // Debounce in software
    uint32_t _lastDebounceTime = 0;
    bool _lastReading = true;  // HIGH = not pressed (pull-up)
    bool _debouncedState = true;

    void emitEvent(ButtonEvent event);
};

// Static instance pointer for ISR
ButtonHandler* ButtonHandler::_instance = nullptr;

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void IRAM_ATTR ButtonHandler::buttonISR() {
    if (_instance == nullptr) return;

    uint32_t now = millis();
    bool pressed = !digitalRead(_instance->_pin);  // Active low

    if (pressed) {
        _instance->_isrPressTime = now;
        _instance->_isrPressEvent = true;
        _instance->_isrPressed = true;
    } else {
        _instance->_isrReleaseTime = now;
        _instance->_isrReleaseEvent = true;
        _instance->_isrPressed = false;
    }
}

inline void ButtonHandler::begin(uint8_t pin) {
    _pin = pin;
    _instance = this;

    pinMode(pin, INPUT_PULLUP);

    // Read initial state
    _debouncedState = digitalRead(pin);
    _lastReading = _debouncedState;
    _isrPressed = !_debouncedState;

    // Attach interrupt on both edges
    attachInterrupt(digitalPinToInterrupt(pin), buttonISR, CHANGE);

    DEBUG_PRINTF("[BTN] Initialized with interrupt on GPIO%d\n", pin);
}

inline void ButtonHandler::update() {
    uint32_t now = millis();

    // Process ISR-captured press event
    if (_isrPressEvent) {
        _isrPressEvent = false;

        // Debounce check
        if ((now - _lastDebounceTime) >= BTN_DEBOUNCE_MS) {
            _lastDebounceTime = now;
            _pressTime = _isrPressTime;

            if (_state == State::IDLE) {
                _state = State::PRESSED;
                DEBUG_PRINTLN("[BTN] Press detected");
            } else if (_state == State::WAIT_DOUBLE) {
                // Second press for double click
                _pressTime = _isrPressTime;
                _clickCount = 2;
                _state = State::PRESSED;
            }
        }
    }

    // Process ISR-captured release event
    if (_isrReleaseEvent) {
        _isrReleaseEvent = false;

        // Debounce check
        if ((now - _lastDebounceTime) >= BTN_DEBOUNCE_MS) {
            _lastDebounceTime = now;
            _releaseTime = _isrReleaseTime;

            if (_state == State::PRESSED) {
                uint32_t pressDuration = _releaseTime - _pressTime;

                if (pressDuration >= BTN_VERY_LONG_PRESS_MS) {
                    emitEvent(ButtonEvent::VERY_LONG_PRESS);
                    _state = State::IDLE;
                } else if (pressDuration >= BTN_LONG_PRESS_MS) {
                    emitEvent(ButtonEvent::LONG_PRESS);
                    _state = State::IDLE;
                } else {
                    // Short press - wait for possible double click
                    if (_clickCount == 2) {
                        // This was the second press of a double-click
                        emitEvent(ButtonEvent::DOUBLE_CLICK);
                        _clickCount = 0;
                        _state = State::IDLE;
                    } else {
                        _clickCount = 1;
                        _state = State::WAIT_DOUBLE;
                    }
                }
            } else if (_state == State::LONG_PRESSING) {
                _state = State::IDLE;
            }
        }
    }

    // Time-based state transitions
    switch (_state) {
        case State::IDLE:
            break;

        case State::PRESSED:
            // Check for long/very long press while still holding
            if (_isrPressed) {
                uint32_t holdTime = now - _pressTime;
                if (holdTime >= BTN_VERY_LONG_PRESS_MS) {
                    emitEvent(ButtonEvent::VERY_LONG_PRESS);
                    _state = State::LONG_PRESSING;
                } else if (holdTime >= BTN_LONG_PRESS_MS) {
                    emitEvent(ButtonEvent::LONG_PRESS);
                    _state = State::LONG_PRESSING;
                }
            }
            break;

        case State::WAIT_DOUBLE:
            // Timeout waiting for double click
            if ((now - _releaseTime) > BTN_DOUBLE_CLICK_MS) {
                emitEvent(ButtonEvent::SINGLE_CLICK);
                _clickCount = 0;
                _state = State::IDLE;
            }
            break;

        case State::LONG_PRESSING:
            // Waiting for release (handled in ISR release processing)
            break;
    }
}

inline ButtonEvent ButtonHandler::getEvent() {
    ButtonEvent event = _pendingEvent;
    _pendingEvent = ButtonEvent::NONE;
    return event;
}

inline bool ButtonHandler::isPressed() {
    return _isrPressed;
}

inline void ButtonHandler::emitEvent(ButtonEvent event) {
    _pendingEvent = event;

    if (_callback) {
        _callback(event);
    }

    #ifdef CORE_DEBUG_LEVEL
    #if CORE_DEBUG_LEVEL >= 3
    const char* eventNames[] = {"NONE", "SINGLE", "DOUBLE", "LONG", "VERY_LONG"};
    DEBUG_PRINTF("[BTN] Event: %s\n", eventNames[(int)event]);
    #endif
    #endif
}

#endif // BUTTON_HANDLER_H
