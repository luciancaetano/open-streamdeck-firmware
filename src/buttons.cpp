// ============================================================================
// Buttons Module - Implementation
// ============================================================================

#include "buttons.h"
#include <ArduinoJson.h>

// Per-button debounce and state tracking
struct ButtonState {
    bool     pressed;       // Current debounced state
    bool     lastRaw;       // Last raw reading (for edge detection)
    uint32_t lastChangeMs;  // Timestamp of last raw transition
    uint32_t pressStartMs;  // When the current press began
    bool     longFired;     // Whether long-press was already emitted
};

static ButtonState  state[NUM_BUTTONS];
static ButtonEventCallback eventCb = nullptr;

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void buttons_init() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        state[i] = { false, true, 0, 0, false };
    }
}

void buttons_set_callback(ButtonEventCallback cb) {
    eventCb = cb;
}

void buttons_scan() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool raw = !digitalRead(BUTTON_PINS[i]);  // Active LOW

        // Track raw transitions for debounce timing
        if (raw != state[i].lastRaw) {
            state[i].lastRaw      = raw;
            state[i].lastChangeMs = now;
        }

        // Only accept state change after signal is stable for DEBOUNCE_MS
        if ((now - state[i].lastChangeMs) < DEBOUNCE_MS) continue;

        if (raw && !state[i].pressed) {
            // Rising edge: button just pressed
            state[i].pressed     = true;
            state[i].pressStartMs = now;
            state[i].longFired   = false;

            if (eventCb) {
                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["btn"]    = i;
                doc["action"] = "pressed";
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                eventCb(buf);
            }
        }
        else if (raw && state[i].pressed && !state[i].longFired) {
            // Still held: check for long press
            if ((now - state[i].pressStartMs) >= LONG_PRESS_MS) {
                state[i].longFired = true;

                if (eventCb) {
                    StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                    doc["btn"]    = i;
                    doc["action"] = "long_pressed";
                    char buf[JSON_TX_BUFFER_SIZE];
                    serializeJson(doc, buf);
                    eventCb(buf);
                }
            }
        }
        else if (!raw && state[i].pressed) {
            // Falling edge: button released
            state[i].pressed = false;

            if (eventCb) {
                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["btn"]    = i;
                doc["action"] = "released";
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                eventCb(buf);
            }
        }
    }
}

uint64_t buttons_get_wakeup_mask() {
    uint64_t mask = 0;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (BUTTON_PINS[i] < 34) {  // Only RTC GPIOs support ext1 wakeup
            mask |= (1ULL << BUTTON_PINS[i]);
        }
    }
    return mask;
}
