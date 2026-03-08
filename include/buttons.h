#pragma once
// ============================================================================
// Buttons Module - Debounced press/release detection for BLE HID
// ============================================================================

#include "config.h"

enum ButtonAction { BTN_PRESSED, BTN_RELEASED };

typedef void (*ButtonEventCallback)(uint8_t index, ButtonAction action);

void buttons_init();
void buttons_set_callback(ButtonEventCallback cb);
void buttons_scan();
uint64_t buttons_get_wakeup_mask();
