#pragma once
// ============================================================================
// LEDs Module - RGB LED control and host command handling
// ============================================================================
//
// Manages WS2812B LED strip feedback:
//   - Idle color, press flash, host-set colors
//   - Handles incoming JSON commands for LED control
//   - Provides sleep/wake brightness management
//
// All LED logic is compiled out when LED_ENABLED == 0.
// ============================================================================

#include "config.h"

// Initialize the LED strip hardware and set default colors.
void leds_init();

// Refresh the LED strip (apply flash overlays, target colors).
// Should be called at LED_UPDATE_INTERVAL from the main loop.
void leds_update();

// Trigger a brief white flash on the LED at the given button index.
void leds_flash_button(int btnIndex);

// Handle an incoming JSON command that may affect LEDs.
// Returns true if the command was consumed (was LED-related).
bool leds_handle_command(const char* json);

// Dim LEDs to 0 before entering sleep mode.
void leds_sleep();

// Restore brightness after waking from sleep.
void leds_wake();
