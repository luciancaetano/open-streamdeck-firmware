// ============================================================================
// Slider Module - Implementation
// ============================================================================

#include "slider.h"
#include <ArduinoJson.h>

// Per-slider runtime state
struct SliderState {
    int32_t smoothed;      // EMA-smoothed ADC value
    int32_t lastReported;  // Last value sent to host
    bool    initialized;   // First-read flag (seed EMA on first sample)
};

static SliderState         state[NUM_SLIDERS];
static SliderEventCallback eventCb = nullptr;

// EMA smoothing factor via bit-shift: smoothed += (raw - smoothed) >> EMA_SHIFT
// Shift=2 means alpha ~0.25 — good balance between noise filtering and response.
#define EMA_SHIFT 2

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void slider_init() {
    for (int i = 0; i < NUM_SLIDERS; i++) {
        state[i].smoothed     = 0;
        state[i].lastReported = -1;  // Force first report
        state[i].initialized  = false;
    }
}

void slider_set_callback(SliderEventCallback cb) {
    eventCb = cb;
}

void slider_scan() {
    for (int i = 0; i < NUM_SLIDERS; i++) {
        int32_t raw = analogRead(SLIDER_CONFIGS[i].adcPin);

        // Seed EMA on first read
        if (!state[i].initialized) {
            state[i].smoothed    = raw;
            state[i].initialized = true;
        } else {
            // Integer EMA: avoids float math on ESP32
            state[i].smoothed += (raw - state[i].smoothed) >> EMA_SHIFT;
        }

        int32_t value = state[i].smoothed;

        // Only emit when change exceeds threshold (filters ADC noise)
        if (abs(value - state[i].lastReported) >= (int32_t)SLIDER_CONFIGS[i].threshold) {
            state[i].lastReported = value;

            if (eventCb) {
                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["slider"] = SLIDER_CONFIGS[i].id;
                doc["value"]  = value;
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                eventCb(buf);
            }
        }
    }
}
