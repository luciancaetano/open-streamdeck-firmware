#pragma once
// ============================================================================
// Open StreamDeck - Configuration
// ESP32 DOIT DevKit V1 | 12 Buttons + 1 Rotary Encoder + Optional RGB LEDs
// ============================================================================
//
// All hardware pin mappings, timing constants, and feature toggles live here.
// Modify this file to adapt the firmware to different hardware configurations.
// ============================================================================

#include <Arduino.h>

// ----------------------------------------------------------------------------
// BLE Configuration
// ----------------------------------------------------------------------------
#define BLE_DEVICE_NAME       "OpenStreamDeck"
#define BLE_SERVICE_UUID      "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // Nordic UART
#define BLE_TX_CHAR_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"  // TX (notify)
#define BLE_RX_CHAR_UUID      "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"  // RX (write)

// ----------------------------------------------------------------------------
// USB Serial Configuration
// ----------------------------------------------------------------------------
#define USB_SERIAL_BAUD       115200

// ----------------------------------------------------------------------------
// Button Configuration (active LOW with internal pull-up)
// ----------------------------------------------------------------------------
#define NUM_BUTTONS           12
#define DEBOUNCE_MS           30      // Debounce interval in milliseconds
#define LONG_PRESS_MS         600     // Long-press threshold

// GPIO pins for 12 buttons - mapped to safe ESP32 DOIT DevKit V1 GPIOs
// Avoids: GPIO0 (boot), GPIO2 (onboard LED), GPIO6-11 (flash),
//         GPIO34-39 (input-only, no pull-up)
static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    4,    // BTN 0
    5,    // BTN 1
    13,   // BTN 2
    14,   // BTN 3
    15,   // BTN 4
    16,   // BTN 5
    17,   // BTN 6
    18,   // BTN 7
    19,   // BTN 8
    21,   // BTN 9
    22,   // BTN 10
    23    // BTN 11
};

// ----------------------------------------------------------------------------
// Rotary Encoder / Knob Configuration
// ----------------------------------------------------------------------------
// Each knob is defined by its pins and a string ID used in JSON events.
// To add more knobs, increase NUM_KNOBS and extend the KNOB_CONFIGS array.

#define NUM_KNOBS             1
#define ENCODER_DEBOUNCE_MS   30      // Debounce for encoder push button
#define ENCODER_THRESHOLD     1       // Minimum count delta to register a tick

struct KnobConfig {
    const char* id;       // JSON identifier (e.g. "volume")
    uint8_t     pinA;     // CLK pin
    uint8_t     pinB;     // DT pin
    uint8_t     btnPin;   // SW (push button) pin
};

static const KnobConfig KNOB_CONFIGS[NUM_KNOBS] = {
    { "volume", 32, 33, 25 },
};

// ----------------------------------------------------------------------------
// RGB LED Configuration (WS2812B / NeoPixel via FastLED)
// ----------------------------------------------------------------------------
#define LED_ENABLED           0       // Set to 1 to enable LED support
#define LED_DATA_PIN          26      // Data pin for WS2812B strip
#define NUM_LEDS              12      // One LED per button
#define LED_BRIGHTNESS        40      // Default brightness (0-255)
#define LED_TYPE              WS2812B
#define LED_COLOR_ORDER       GRB

// Default LED colors (as 0xRRGGBB)
#define LED_COLOR_IDLE        0x001010  // Dim cyan when idle
#define LED_COLOR_PRESSED     0xFFFFFF  // White flash on press
#define LED_COLOR_OFF         0x000000

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
#define IDLE_TIMEOUT_MS       30000   // Enter light sleep after 30s of inactivity
#define SLEEP_CHECK_INTERVAL  1000    // How often to check idle state (ms)
#define BLE_ADV_INTERVAL_MIN  160     // 100ms  (units of 0.625ms)
#define BLE_ADV_INTERVAL_MAX  320     // 200ms  (units of 0.625ms)

// ----------------------------------------------------------------------------
// Scan & Loop Timing
// ----------------------------------------------------------------------------
#define BUTTON_SCAN_INTERVAL  5       // Scan buttons every 5ms
#define ENCODER_SCAN_INTERVAL 2       // Read encoder every 2ms
#define LED_UPDATE_INTERVAL   16      // ~60fps LED refresh
#define SERIAL_RX_INTERVAL    10      // Check for incoming commands every 10ms

// ----------------------------------------------------------------------------
// JSON Buffer Sizes
// ----------------------------------------------------------------------------
#define JSON_TX_BUFFER_SIZE   128     // Outgoing event buffer
#define JSON_RX_BUFFER_SIZE   256     // Incoming command buffer
#define SERIAL_RX_LINE_MAX    256     // Max incoming line length

// ----------------------------------------------------------------------------
// Named Color Map (for host commands like {"btn":3,"led":"red"})
// ----------------------------------------------------------------------------
struct NamedColor {
    const char* name;
    uint32_t    color;
};

static const NamedColor COLOR_MAP[] = {
    { "red",     0xFF0000 },
    { "green",   0x00FF00 },
    { "blue",    0x0000FF },
    { "yellow",  0xFFFF00 },
    { "cyan",    0x00FFFF },
    { "magenta", 0xFF00FF },
    { "white",   0xFFFFFF },
    { "orange",  0xFF8000 },
    { "purple",  0x8000FF },
    { "off",     0x000000 },
};
static const size_t COLOR_MAP_SIZE = sizeof(COLOR_MAP) / sizeof(COLOR_MAP[0]);
