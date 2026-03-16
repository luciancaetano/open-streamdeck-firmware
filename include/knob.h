#pragma once
// ============================================================================
// Knob Module - Rotary encoder with volume control + multi-click detection
// ============================================================================
//
// Rotation: calls rotation callback with direction (+1 = up, -1 = down)
// Button:   press/release callback + multi-click detection (1, 2, or 3 clicks)
// ============================================================================

#include "config.h"

typedef void (*KnobRotationCallback)(int direction);    // +1 or -1
typedef void (*KnobButtonCallback)(bool pressed);       // true=pressed, false=released
typedef void (*KnobClickCallback)(uint8_t clickCount);  // 1, 2, or 3

void knob_init();
void knob_set_rotation_callback(KnobRotationCallback cb);
void knob_set_button_callback(KnobButtonCallback cb);
void knob_set_click_callback(KnobClickCallback cb);
void knob_scan_rotation();
void knob_scan_button();
uint64_t knob_get_wakeup_mask();
