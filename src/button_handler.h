#ifndef BUTTON_HANDLER_H
#define BUTTON_HANDLER_H

#include <Arduino.h>
#include "config.h"

// =============================================================================
// BUTTON EVENTS (simplified - short press and long press only)
// =============================================================================
enum class ButtonEvent : uint8_t {
    NONE            = 0,
    SINGLE_CLICK    = 1,    // Short press < 2 sec (immediate on release)
    LONG_PRESS      = 2     // Hold >= 2 sec (emitted while holding)
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
static volatile DRAM_ATTR uint32_t _btnIsrLastChange = 0;

// Hardware debounce threshold in ISR
#define ISR_DEBOUNCE_MS 30

// =============================================================================
// ISR HANDLER (in IRAM, accesses only DRAM static variables)
// =============================================================================
static void IRAM_ATTR buttonISR() {
    uint32_t now = millis();

    // Quick hardware debounce in ISR
    if ((now - _btnIsrLastChange) < ISR_DEBOUNCE_MS) {
        return;  // Ignore bounces
    }
    _btnIsrLastChange = now;

    bool pressed = !digitalRead(_btnIsrPin);  // Active low

    if (pressed && !_btnIsrPressed) {
        _btnIsrPressTime = now;
        _btnIsrPressEvent = true;
        _btnIsrPressed = true;
    } else if (!pressed && _btnIsrPressed) {
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

    // Simple state machine
    enum class State {
        IDLE,
        PRESSED,
        LONG_FIRED      // Long press already fired, waiting for release
    };

    State _state = State::IDLE;
    ButtonEvent _pendingEvent = ButtonEvent::NONE;
    uint32_t _pressTime = 0;

    EventCallback _callback = nullptr;

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

    DEBUG_PRINTF("[BTN] Initialized on GPIO%d\n", pin);
}

inline void ButtonHandler::update() {
    uint32_t now = millis();

    // Safety: Reset stuck state machine
    if (_state != State::IDLE && !_btnIsrPressed && (now - _pressTime) > 10000) {
        DEBUG_PRINTLN("[BTN] State machine reset (timeout)");
        _state = State::IDLE;
    }

    // Process ISR-captured press event
    if (_btnIsrPressEvent) {
        _btnIsrPressEvent = false;
        _pressTime = _btnIsrPressTime;

        if (_state == State::IDLE) {
            _state = State::PRESSED;
            DEBUG_PRINTLN("[BTN] Press detected");
        }
    }

    // Process ISR-captured release event
    if (_btnIsrReleaseEvent) {
        _btnIsrReleaseEvent = false;

        if (_state == State::PRESSED) {
            // Short press - emit single click immediately
            emitEvent(ButtonEvent::SINGLE_CLICK);
        }
        // If LONG_FIRED, the event was already emitted
        _state = State::IDLE;
    }

    // Check for long press while still holding
    if (_state == State::PRESSED && _btnIsrPressed) {
        uint32_t holdTime = now - _pressTime;

        if (holdTime >= BTN_LONG_PRESS_MS) {
            emitEvent(ButtonEvent::LONG_PRESS);
            _state = State::LONG_FIRED;
        }
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
        event == ButtonEvent::LONG_PRESS ? "LONG" : "NONE");
}

#endif // BUTTON_HANDLER_H
