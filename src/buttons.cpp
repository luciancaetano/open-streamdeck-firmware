// ============================================================================
// Buttons Module - Implementation
// ============================================================================

#include "buttons.h"

struct ButtonState {
    bool     pressed;
    bool     lastRaw;
    uint32_t lastChangeMs;
};

static ButtonState state[NUM_BUTTONS];
static ButtonEventCallback eventCb = nullptr;

void buttons_init() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        state[i] = { false, true, 0 };
    }
}

void buttons_set_callback(ButtonEventCallback cb) {
    eventCb = cb;
}

void buttons_scan() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool raw = !digitalRead(BUTTON_PINS[i]);  // Active LOW

        if (raw != state[i].lastRaw) {
            state[i].lastRaw      = raw;
            state[i].lastChangeMs = now;
        }

        if ((now - state[i].lastChangeMs) < DEBOUNCE_MS) continue;

        if (raw && !state[i].pressed) {
            state[i].pressed = true;
            if (eventCb) eventCb(i, BTN_PRESSED);
        }
        else if (!raw && state[i].pressed) {
            state[i].pressed = false;
            if (eventCb) eventCb(i, BTN_RELEASED);
        }
    }
}

uint64_t buttons_get_wakeup_mask() {
    uint64_t mask = 0;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (BUTTON_PINS[i] < 34) {
            mask |= (1ULL << BUTTON_PINS[i]);
        }
    }
    return mask;
}
