#pragma once
// ============================================================================
// Knob Module - Rotary encoder scanning, rotation & press events
// ============================================================================
//
// Supports NUM_KNOBS rotary encoders, each with CLK/DT/SW pins.
// Generates JSON events:
//   Rotation: {"knob": "<id>", "direction": "up|down"}
//   Press:    {"knob": "<id>", "pressed": true|false}
//
// Designed for easy expansion: add entries to KNOB_CONFIGS in config.h.
// ============================================================================

#include "config.h"

// Callback type: called with a serialized JSON string on knob events
typedef void (*KnobEventCallback)(const char* json);

// Initialize encoder hardware and internal state for all knobs.
void knob_init();

// Register a callback for knob events.
void knob_set_callback(KnobEventCallback cb);

// Scan all knobs for rotation changes.
// Should be called at ENCODER_SCAN_INTERVAL from the main loop.
void knob_scan_rotation();

// Scan all knob push-buttons for press/release.
// Can be called at BUTTON_SCAN_INTERVAL (shares the same timing).
void knob_scan_buttons();

// Return GPIO bitmask of all knob button pins (for sleep wakeup).
uint64_t knob_get_wakeup_mask();
