// ============================================================================
// Open StreamDeck Firmware - Main Entry Point
// ESP32 DOIT DevKit V1 | 12 Buttons + Rotary Encoder + BT/USB Serial
// ============================================================================
//
// This file wires all modules together:
//   buttons  -> scans physical buttons, emits JSON events
//   knob     -> scans rotary encoders, emits JSON events
//   slider   -> scans analog faders, emits JSON events
//   comms    -> centralized protocol layer (encode, decode, ACK, routing)
//   leds     -> RGB LED feedback and host-driven color control
//
// The main loop is non-blocking and timer-gated for each subsystem.
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include "buttons.h"
#include "knob.h"
#include "slider.h"
#include "comms.h"
#include "leds.h"
#include <ArduinoJson.h>

// ----------------------------------------------------------------------------
// Timing State
// ----------------------------------------------------------------------------
static uint32_t lastButtonScan  = 0;
static uint32_t lastEncoderScan = 0;
static uint32_t lastSliderScan  = 0;
static uint32_t lastLedUpdate   = 0;
static uint32_t lastSerialRx    = 0;
static uint32_t lastHeartbeat   = 0;
static uint32_t lastSleepCheck  = 0;
static uint32_t lastActivityMs  = 0;

// Heartbeat uptime tracking (survives millis() wrap)
static uint32_t uptimeSeconds   = 0;
static uint32_t lastUptimeTick  = 0;

// ----------------------------------------------------------------------------
// Idle Timer
// ----------------------------------------------------------------------------
static void resetIdleTimer() {
    lastActivityMs = millis();
}

// ----------------------------------------------------------------------------
// Event Dispatch: send via comms and reset idle timer
// ----------------------------------------------------------------------------
static void onInputEvent(const char* json) {
    resetIdleTimer();
    comms_send_event(json);
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
// Command Handler: try each module, return command name or nullptr.
// Comms layer handles JSON parsing and ACK generation automatically.
// ----------------------------------------------------------------------------
static const char* onCommand(const char* json) {
    resetIdleTimer();

    // Try LED module
    const char* cmdName = leds_handle_command(json);
    if (cmdName) return cmdName;

    // Future modules go here:
    // cmdName = macros_handle_command(json);
    // if (cmdName) return cmdName;

    return nullptr;  // Comms will send {"ack":false,"error":"unknown_command"}
}

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
static void checkIdleSleep() {
    if (comms_bt_connected()) return;
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
    comms_init();           // Initializes USB + BLE transports
    buttons_init();
    knob_init();
    slider_init();
    leds_init();

    // Wire input event callbacks
    buttons_set_callback(onButtonEvent);
    knob_set_callback(onInputEvent);
    slider_set_callback(onInputEvent);

    // Wire command handler (comms handles parse + ACK around it)
    comms_set_command_callback(onCommand);

    lastActivityMs = millis();
    lastUptimeTick = millis();
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

    if (now - lastSliderScan >= SLIDER_SCAN_INTERVAL) {
        lastSliderScan = now;
        slider_scan();
    }

    if (now - lastSerialRx >= SERIAL_RX_INTERVAL) {
        lastSerialRx = now;
        comms_process_rx();
    }

#if LED_ENABLED
    if (now - lastLedUpdate >= LED_UPDATE_INTERVAL) {
        lastLedUpdate = now;
        leds_update();
    }
#endif

    if (now - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        lastHeartbeat = now;
        // Accumulate elapsed seconds (handles millis wrap)
        uptimeSeconds += (now - lastUptimeTick) / 1000;
        lastUptimeTick = now;
        comms_send_heartbeat(uptimeSeconds);
    }

    if (now - lastSleepCheck >= SLEEP_CHECK_INTERVAL) {
        lastSleepCheck = now;
        checkIdleSleep();
    }

    delay(1);  // Yield to FreeRTOS / watchdog
}
