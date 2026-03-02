// ============================================================================
// Open StreamDeck Firmware - Main Entry Point
// ESP32 DOIT DevKit V1 | 12 Buttons + Rotary Encoder + BLE/USB Serial
// ============================================================================
//
// This file wires all modules together:
//   buttons    -> scans physical buttons, emits JSON events
//   knob       -> scans rotary encoders, emits JSON events
//   ble_serial -> BLE Nordic UART transport (send/receive)
//   usb_serial -> USB hardware serial transport (send/receive)
//   leds       -> RGB LED feedback and host-driven color control
//
// The main loop is non-blocking and timer-gated for each subsystem.
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "buttons.h"
#include "knob.h"
#include "ble_serial.h"
#include "usb_serial.h"
#include "leds.h"
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------
// Timing State
// ----------------------------------------------------------------------------
static uint32_t lastButtonScan  = 0;
static uint32_t lastEncoderScan = 0;
static uint32_t lastLedUpdate   = 0;
static uint32_t lastSerialRx    = 0;
static uint32_t lastSleepCheck  = 0;
static uint32_t lastActivityMs  = 0;

// ----------------------------------------------------------------------------
// Idle Timer
// ----------------------------------------------------------------------------
static void resetIdleTimer() {
    lastActivityMs = millis();
}

// ----------------------------------------------------------------------------
// Event Dispatch: send to both USB and BLE, reset idle timer
// ----------------------------------------------------------------------------
static void onInputEvent(const char* json) {
    resetIdleTimer();
    usb_send(json);
    ble_send(json);
}

// ----------------------------------------------------------------------------
// Button event handler: forward event + trigger LED flash on press
// ----------------------------------------------------------------------------
static void onButtonEvent(const char* json) {
    onInputEvent(json);

    // Flash the corresponding LED on press
    StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
    if (deserializeJson(doc, json) == DeserializationError::Ok) {
        if (doc.containsKey("btn") && doc["action"] == "pressed") {
            leds_flash_button(doc["btn"].as<int>());
        }
    }
}

// ----------------------------------------------------------------------------
// Incoming Command Handler (from USB or BLE)
// ----------------------------------------------------------------------------
static void onCommand(const char* json) {
    resetIdleTimer();

    // Try LED module first; if not consumed, log unknown command
    if (leds_handle_command(json)) return;

    Serial.print("[CMD] Unknown command: ");
    Serial.println(json);
}

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
static void checkIdleSleep() {
    if (ble_is_connected()) return;
    if (Serial.available()) return;

    uint32_t idleMs = millis() - lastActivityMs;
    if (idleMs < IDLE_TIMEOUT_MS) return;

    // Prepare for light sleep
    leds_sleep();

    // Build wakeup GPIO mask from buttons + knob buttons
    uint64_t gpioMask = buttons_get_wakeup_mask() | knob_get_wakeup_mask();
    if (gpioMask) {
        esp_sleep_enable_ext1_wakeup(gpioMask, ESP_EXT1_WAKEUP_ALL_LOW);
    }
    // Timer wakeup as safety net (e.g. BLE reconnect check)
    esp_sleep_enable_timer_wakeup(5000000);  // 5 seconds

    esp_light_sleep_start();

    // -- Woke up --
    resetIdleTimer();
    leds_wake();
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    usb_init();
    buttons_init();
    knob_init();
    leds_init();
    ble_init();

    // Wire event callbacks
    buttons_set_callback(onButtonEvent);
    knob_set_callback(onInputEvent);
    usb_set_rx_callback(onCommand);
    ble_set_rx_callback(onCommand);

    lastActivityMs = millis();
}

// ============================================================================
// Main Loop - Non-blocking, timer-gated
// ============================================================================
void loop() {
    uint32_t now = millis();

    if (now - lastButtonScan >= BUTTON_SCAN_INTERVAL) {
        lastButtonScan = now;
        buttons_scan();
        knob_scan_buttons();
    }

    if (now - lastEncoderScan >= ENCODER_SCAN_INTERVAL) {
        lastEncoderScan = now;
        knob_scan_rotation();
    }

    if (now - lastSerialRx >= SERIAL_RX_INTERVAL) {
        lastSerialRx = now;
        usb_process_rx();
        ble_process_rx();
    }

#if LED_ENABLED
    if (now - lastLedUpdate >= LED_UPDATE_INTERVAL) {
        lastLedUpdate = now;
        leds_update();
    }
#endif

    if (now - lastSleepCheck >= SLEEP_CHECK_INTERVAL) {
        lastSleepCheck = now;
        checkIdleSleep();
    }

    delay(1);  // Yield to FreeRTOS / watchdog
}
