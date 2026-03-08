#pragma once
// ============================================================================
// Knob Module - Rotary encoder with volume control + multi-click detection
// ============================================================================
//
// Rotation: calls volume callback with direction (+1 = up, -1 = down)
// Button:   detects multi-click (1=play/pause, 2=next, 3=prev)
// ============================================================================

#include "config.h"

typedef void (*KnobRotationCallback)(int direction);  // +1 or -1
typedef void (*KnobClickCallback)(uint8_t clickCount); // 1, 2, or 3

void knob_init();
void knob_set_rotation_callback(KnobRotationCallback cb);
void knob_set_click_callback(KnobClickCallback cb);
void knob_scan_rotation();
void knob_scan_button();
uint64_t knob_get_wakeup_mask();
