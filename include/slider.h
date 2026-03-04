#pragma once
// ============================================================================
// Slider Module - Analog fader/potentiometer scanning
// ============================================================================
//
// Supports NUM_SLIDERS analog inputs on ADC-capable pins (GPIO 34-39).
// Uses EMA smoothing and threshold-based change detection to filter noise.
// Generates JSON events:
//   {"slider": "<id>", "value": <0-4095>}
// ============================================================================

#include "config.h"

// Callback type: called with a serialized JSON string on slider events
typedef void (*SliderEventCallback)(const char* json);

// Initialize ADC pins and internal state for all sliders.
void slider_init();

// Register a callback for slider events.
void slider_set_callback(SliderEventCallback cb);

// Read all slider ADC values, apply smoothing, emit events if changed.
// Should be called at SLIDER_SCAN_INTERVAL from the main loop.
void slider_scan();
