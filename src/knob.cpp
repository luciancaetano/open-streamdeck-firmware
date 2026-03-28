// ============================================================================
// Knob Module - Implementation
// ============================================================================

#include "knob.h"
#include <ESP32Encoder.h>

#ifndef DISABLE_KNOB

static ESP32Encoder encoder;
static int64_t lastCount = 0;

// Button multi-click state
static bool     btnPressed    = false;
static bool     btnLastRaw    = true;   // Idle HIGH (pull-up)
static uint32_t btnLastChange = 0;
static uint8_t  clickCount    = 0;
static uint32_t lastReleaseMs = 0;
static bool     waitingClicks = false;

static KnobRotationCallback rotCb   = nullptr;
static KnobButtonCallback   btnCb   = nullptr;
static KnobClickCallback    clickCb = nullptr;

void knob_init() {
    ESP32Encoder::useInternalWeakPullResistors = UP;
    encoder.attachSingleEdge(ENCODER_PIN_A, ENCODER_PIN_B);
    encoder.setCount(0);
    lastCount = 0;

    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
}

void knob_set_rotation_callback(KnobRotationCallback cb) {
    rotCb = cb;
}

void knob_set_button_callback(KnobButtonCallback cb) {
    btnCb = cb;
}

void knob_set_click_callback(KnobClickCallback cb) {
    clickCb = cb;
}

void knob_scan_rotation() {
    int64_t count = encoder.getCount();
    int64_t delta = count - lastCount;

    if (abs((int)delta) >= ENCODER_THRESHOLD) {
        lastCount = count;
        if (rotCb) {
            rotCb(delta > 0 ? 1 : -1);
        }
    }
}

void knob_scan_button() {
    uint32_t now = millis();
    bool raw = !digitalRead(ENCODER_BTN_PIN);  // Active LOW

    // Debounce
    if (raw != btnLastRaw) {
        btnLastRaw    = raw;
        btnLastChange = now;
    }

    if ((now - btnLastChange) >= ENCODER_DEBOUNCE_MS) {
        if (raw && !btnPressed) {
            // Press detected
            btnPressed = true;
            if (btnCb) btnCb(true);
        }
        else if (!raw && btnPressed) {
            // Release detected -> count this click
            btnPressed    = false;
            if (btnCb) btnCb(false);
            clickCount++;
            lastReleaseMs = now;
            waitingClicks = true;
        }
    }

    // Check if multi-click window has expired
    if (waitingClicks && (now - lastReleaseMs) >= MULTI_CLICK_TIMEOUT) {
        waitingClicks = false;
        if (clickCb && clickCount > 0) {
            clickCb(clickCount > 3 ? 3 : clickCount);
        }
        clickCount = 0;
    }
}

uint64_t knob_get_wakeup_mask() {
    if (ENCODER_BTN_PIN < 34) {
        return (1ULL << ENCODER_BTN_PIN);
    }
    return 0;
}

#else

void knob_init() {
    // Knob disabled by DISABLE_KNOB
}

void knob_set_rotation_callback(KnobRotationCallback cb) {
    (void)cb;
}

void knob_set_button_callback(KnobButtonCallback cb) {
    (void)cb;
}

void knob_set_click_callback(KnobClickCallback cb) {
    (void)cb;
}

void knob_scan_rotation() {
    // Knob disabled
}

void knob_scan_button() {
    // Knob disabled
}

uint64_t knob_get_wakeup_mask() {
    return 0;
}

#endif
