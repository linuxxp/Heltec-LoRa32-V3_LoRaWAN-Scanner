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
// ISR-SAFE STATIC VARIABLES (must be in DRAM for IRAM access)
// =============================================================================
static volatile DRAM_ATTR bool _btnIsrPressed = false;
static volatile DRAM_ATTR uint32_t _btnIsrPressTime = 0;
static volatile DRAM_ATTR uint32_t _btnIsrReleaseTime = 0;
static volatile DRAM_ATTR bool _btnIsrPressEvent = false;
static volatile DRAM_ATTR bool _btnIsrReleaseEvent = false;
static volatile DRAM_ATTR uint8_t _btnIsrPin = 0;

// =============================================================================
// ISR HANDLER (in IRAM, accesses only DRAM static variables)
// =============================================================================
static void IRAM_ATTR buttonISR() {
    uint32_t now = millis();
    bool pressed = !digitalRead(_btnIsrPin);  // Active low

    if (pressed) {
        _btnIsrPressTime = now;
        _btnIsrPressEvent = true;
        _btnIsrPressed = true;
    } else {
        _btnIsrReleaseTime = now;
        _btnIsrReleaseEvent = true;
        _btnIsrPressed = false;
    }
}

// =============================================================================
// BUTTON HANDLER CLASS
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

    // Debounce
    uint32_t _lastDebounceTime = 0;

    void emitEvent(ButtonEvent event);
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void ButtonHandler::begin(uint8_t pin) {
    _pin = pin;
    _btnIsrPin = pin;  // Store in static for ISR access

    pinMode(pin, INPUT_PULLUP);

    // Read initial state
    _btnIsrPressed = !digitalRead(pin);

    // Attach interrupt on both edges
    attachInterrupt(digitalPinToInterrupt(pin), buttonISR, CHANGE);

    DEBUG_PRINTF("[BTN] Initialized with interrupt on GPIO%d\n", pin);
}

inline void ButtonHandler::update() {
    uint32_t now = millis();

    // Process ISR-captured press event
    if (_btnIsrPressEvent) {
        _btnIsrPressEvent = false;

        // Debounce check
        if ((now - _lastDebounceTime) >= BTN_DEBOUNCE_MS) {
            _lastDebounceTime = now;
            _pressTime = _btnIsrPressTime;

            if (_state == State::IDLE) {
                _state = State::PRESSED;
                DEBUG_PRINTLN("[BTN] Press detected");
            } else if (_state == State::WAIT_DOUBLE) {
                // Second press for double click
                _pressTime = _btnIsrPressTime;
                _clickCount = 2;
                _state = State::PRESSED;
            }
        }
    }

    // Process ISR-captured release event
    if (_btnIsrReleaseEvent) {
        _btnIsrReleaseEvent = false;

        // Debounce check
        if ((now - _lastDebounceTime) >= BTN_DEBOUNCE_MS) {
            _lastDebounceTime = now;
            _releaseTime = _btnIsrReleaseTime;

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
            if (_btnIsrPressed) {
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
    return _btnIsrPressed;
}

inline void ButtonHandler::emitEvent(ButtonEvent event) {
    _pendingEvent = event;

    if (_callback) {
        _callback(event);
    }

    DEBUG_PRINTF("[BTN] Event: %s\n",
        event == ButtonEvent::SINGLE_CLICK ? "SINGLE" :
        event == ButtonEvent::DOUBLE_CLICK ? "DOUBLE" :
        event == ButtonEvent::LONG_PRESS ? "LONG" :
        event == ButtonEvent::VERY_LONG_PRESS ? "VERY_LONG" : "NONE");
}

#endif // BUTTON_HANDLER_H
