#pragma once
// ============================================================================
// Buttons Module - Scanning, debouncing, and event generation
// ============================================================================
//
// Handles NUM_BUTTONS physical buttons with:
//   - Debounced press/release detection
//   - Long-press detection
//   - JSON event generation: {"btn": N, "action": "pressed|released|long_pressed"}
// ============================================================================

#include "config.h"

// Callback type: called with a serialized JSON string whenever a button event fires
typedef void (*ButtonEventCallback)(const char* json);

// Initialize GPIO pins and internal state for all buttons.
// Must be called once during setup().
void buttons_init();

// Register a callback that receives button JSON events.
// Set before calling buttons_scan() to receive events.
void buttons_set_callback(ButtonEventCallback cb);

// Scan all buttons, apply debounce, detect edges and long-press.
// Should be called at BUTTON_SCAN_INTERVAL from the main loop.
void buttons_scan();

// Return the GPIO bitmask of all button pins (for sleep wakeup configuration).
uint64_t buttons_get_wakeup_mask();
