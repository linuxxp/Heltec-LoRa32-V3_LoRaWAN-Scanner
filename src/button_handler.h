#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include <Bounce2.h>
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
    Bounce _button;
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

    void emitEvent(ButtonEvent event);
};

// =============================================================================
// IMPLEMENTATION
// =============================================================================

inline void ButtonHandler::begin(uint8_t pin) {
    _pin = pin;
    _button.attach(pin, INPUT_PULLUP);
    _button.interval(BTN_DEBOUNCE_MS);

    DEBUG_PRINTF("[BTN] Initialized on GPIO%d\n", pin);
}

inline void ButtonHandler::update() {
    _button.update();
    uint32_t now = millis();

    switch (_state) {
        case State::IDLE:
            if (_button.fell()) {
                // Button pressed
                _pressTime = now;
                _state = State::PRESSED;
            }
            break;

        case State::PRESSED:
            if (_button.rose()) {
                // Button released
                uint32_t pressDuration = now - _pressTime;
                _releaseTime = now;

                if (pressDuration >= BTN_VERY_LONG_PRESS_MS) {
                    emitEvent(ButtonEvent::VERY_LONG_PRESS);
                    _state = State::IDLE;
                } else if (pressDuration >= BTN_LONG_PRESS_MS) {
                    emitEvent(ButtonEvent::LONG_PRESS);
                    _state = State::IDLE;
                } else {
                    // Short press - wait for possible double click
                    _clickCount = 1;
                    _state = State::WAIT_DOUBLE;
                }
            } else if ((now - _pressTime) >= BTN_VERY_LONG_PRESS_MS) {
                // Very long press while still holding
                emitEvent(ButtonEvent::VERY_LONG_PRESS);
                _state = State::LONG_PRESSING;
            } else if ((now - _pressTime) >= BTN_LONG_PRESS_MS && _state != State::LONG_PRESSING) {
                // Long press detected, but wait for release or very long
            }
            break;

        case State::WAIT_DOUBLE:
            if (_button.fell()) {
                // Second press started
                _pressTime = now;
                _clickCount = 2;
                _state = State::PRESSED;
            } else if ((now - _releaseTime) > BTN_DOUBLE_CLICK_MS) {
                // Timeout - emit single click
                if (_clickCount == 1) {
                    emitEvent(ButtonEvent::SINGLE_CLICK);
                } else if (_clickCount == 2) {
                    emitEvent(ButtonEvent::DOUBLE_CLICK);
                }
                _clickCount = 0;
                _state = State::IDLE;
            }
            break;

        case State::LONG_PRESSING:
            // Wait for button release after long/very long press
            if (_button.rose()) {
                _state = State::IDLE;
            }
            break;
    }
}

inline ButtonEvent ButtonHandler::getEvent() {
    ButtonEvent event = _pendingEvent;
    _pendingEvent = ButtonEvent::NONE;
    return event;
}

inline bool ButtonHandler::isPressed() {
    return !_button.read();  // Active low
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
