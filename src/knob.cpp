// ============================================================================
// Knob Module - Implementation
// ============================================================================

#include "knob.h"
#include <ESP32Encoder.h>
#include <ArduinoJson.h>

// Per-knob runtime state
struct KnobState {
    ESP32Encoder encoder;
    int64_t      lastCount;   // Last reported encoder count
    // Push-button debounce (same approach as buttons module)
    bool         pressed;
    bool         lastRaw;
    uint32_t     lastChangeMs;
};

static KnobState       knobs[NUM_KNOBS];
static KnobEventCallback eventCb = nullptr;

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void knob_init() {
    ESP32Encoder::useInternalWeakPullResistors = UP;

    for (int i = 0; i < NUM_KNOBS; i++) {
        const KnobConfig& cfg = KNOB_CONFIGS[i];

        knobs[i].encoder.attachSingleEdge(cfg.pinA, cfg.pinB);
        knobs[i].encoder.setCount(0);
        knobs[i].lastCount = 0;

        // Button init
        pinMode(cfg.btnPin, INPUT_PULLUP);
        knobs[i].pressed     = false;
        knobs[i].lastRaw     = true;   // Idle HIGH (pull-up)
        knobs[i].lastChangeMs = 0;
    }
}

void knob_set_callback(KnobEventCallback cb) {
    eventCb = cb;
}

void knob_scan_rotation() {
    for (int i = 0; i < NUM_KNOBS; i++) {
        int64_t count = knobs[i].encoder.getCount();
        int64_t delta = count - knobs[i].lastCount;

        if (abs((int)delta) >= ENCODER_THRESHOLD) {
            knobs[i].lastCount = count;

            if (eventCb) {
                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["knob"]      = KNOB_CONFIGS[i].id;
                doc["direction"] = (delta > 0) ? "up" : "down";
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                eventCb(buf);
            }
        }
    }
}

void knob_scan_buttons() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_KNOBS; i++) {
        bool raw = !digitalRead(KNOB_CONFIGS[i].btnPin);  // Active LOW

        if (raw != knobs[i].lastRaw) {
            knobs[i].lastRaw      = raw;
            knobs[i].lastChangeMs = now;
        }

        if ((now - knobs[i].lastChangeMs) < ENCODER_DEBOUNCE_MS) continue;

        if (raw && !knobs[i].pressed) {
            knobs[i].pressed = true;

            if (eventCb) {
                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["knob"]    = KNOB_CONFIGS[i].id;
                doc["pressed"] = true;
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                eventCb(buf);
            }
        }
        else if (!raw && knobs[i].pressed) {
            knobs[i].pressed = false;

            if (eventCb) {
                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["knob"]    = KNOB_CONFIGS[i].id;
                doc["pressed"] = false;
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                eventCb(buf);
            }
        }
    }
}

uint64_t knob_get_wakeup_mask() {
    uint64_t mask = 0;
    for (int i = 0; i < NUM_KNOBS; i++) {
        if (KNOB_CONFIGS[i].btnPin < 34) {
            mask |= (1ULL << KNOB_CONFIGS[i].btnPin);
        }
    }
    return mask;
}
