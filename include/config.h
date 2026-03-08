#pragma once
// ============================================================================
// Open StreamDeck - Configuration
// ESP32 DOIT DevKit V1 | BLE HID Macro Keyboard
// 8 Buttons + 1 Rotary Encoder (volume + multi-click media control)
// ============================================================================

#include <Arduino.h>

// ----------------------------------------------------------------------------
// Device Identity (shown in Bluetooth pairing)
// ----------------------------------------------------------------------------
#define DEVICE_NAME           "OpenDeck"
#define DEVICE_MANUFACTURER   "OpenDeck"
#define FIRMWARE_VERSION      "2.0.0"

// ----------------------------------------------------------------------------
// Button Configuration (active LOW with internal pull-up)
// ----------------------------------------------------------------------------
#define NUM_BUTTONS           8
#define DEBOUNCE_MS           30

// ESP32 DOIT DevKit V1 - Pin mapping
//
//        ┌──────────────┐
//   3V3  │              │ VIN
//   GND  │              │ GND
//   D15  │              │ D13
//   D2   │              │ D12
//   D4   │              │ D14
//   RX2  │(GPIO16)      │ D27
//   TX2  │(GPIO17)      │ D26
//   D5   │              │ D25  ← ENC SW
//   D18  │              │ D33  ← ENC DT
//   D19  │              │ D32  ← ENC CLK
//   D21  │              │ D35  (input-only)
//   RX0  │(GPIO3)       │ D34  (input-only)
//   TX0  │(GPIO1)       │ VN   (GPIO39, input-only)
//   D22  │              │ VP   (GPIO36, input-only)
//   D23  │              │ EN
//        └──────┬┬──────┘
//               ││ USB
//
// GPIO pins for 8 buttons
static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    4,    // BTN 0  -> pino D4   (lado esquerdo)
    5,    // BTN 1  -> pino D5   (lado esquerdo)
    13,   // BTN 2  -> pino D13  (lado direito)
    14,   // BTN 3  -> pino D14  (lado direito)
    15,   // BTN 4  -> pino D15  (lado esquerdo)
    16,   // BTN 5  -> pino RX2  (lado esquerdo)
    17,   // BTN 6  -> pino TX2  (lado esquerdo)
    18,   // BTN 7  -> pino D18  (lado esquerdo)
};

// HID key codes for each button (F13-F20 by default)
// F13=0xF0, F14=0xF1, ..., F20=0xF7 (ESP32-BLE-Keyboard key codes)
static const uint8_t BUTTON_KEYS[NUM_BUTTONS] = {
    0xF0,  // BTN 0 -> F13
    0xF1,  // BTN 1 -> F14
    0xF2,  // BTN 2 -> F15
    0xF3,  // BTN 3 -> F16
    0xF4,  // BTN 4 -> F17
    0xF5,  // BTN 5 -> F18
    0xF6,  // BTN 6 -> F19
    0xF7,  // BTN 7 -> F20
};

// ----------------------------------------------------------------------------
// Rotary Encoder Configuration
// ----------------------------------------------------------------------------
#define ENCODER_PIN_A         32      // CLK -> pino D32 (lado direito)
#define ENCODER_PIN_B         33      // DT  -> pino D33 (lado direito)
#define ENCODER_BTN_PIN       25      // SW  -> pino D25 (lado direito)
#define ENCODER_DEBOUNCE_MS   30
#define ENCODER_THRESHOLD     1       // Minimum count delta to register a tick

// ----------------------------------------------------------------------------
// Multi-Click Detection (encoder button)
// ----------------------------------------------------------------------------
#define MULTI_CLICK_TIMEOUT   400     // ms to wait for additional clicks

// ----------------------------------------------------------------------------
// Scan & Loop Timing
// ----------------------------------------------------------------------------
#define BUTTON_SCAN_INTERVAL  5       // Scan buttons every 5ms
#define ENCODER_SCAN_INTERVAL 2       // Read encoder every 2ms

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
#define IDLE_TIMEOUT_MS       300000  // 5 min idle before light sleep
#define SLEEP_CHECK_INTERVAL  1000
