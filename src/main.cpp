// ============================================================================
// Open StreamDeck Firmware - BLE HID Macro Keyboard
// ESP32 DOIT DevKit V1 | 10 Buttons + Rotary Encoder
// ============================================================================
//
// Buttons 0-8:  Otemu switches → F13-F21 via BLE HID
// Button 9:     Encoder push button (press/release)
// Encoder:      Rotate = Volume Up/Down
// Enc Btn:      1 click = Play/Pause, 2x = Next, 3x = Prev
// ============================================================================

#include <Arduino.h>
#include <BleKeyboard.h>
#include "config.h"
#include "buttons.h"
#include "knob.h"
#include "serial_protocol.h"

// ----------------------------------------------------------------------------
// BLE Keyboard instance
// ----------------------------------------------------------------------------
BleKeyboard bleKeyboard(DEVICE_NAME, DEVICE_MANUFACTURER, 100);

// ----------------------------------------------------------------------------
// Timing State
// ----------------------------------------------------------------------------
static uint32_t lastButtonScan  = 0;
static uint32_t lastEncoderScan = 0;
static uint32_t lastSleepCheck  = 0;
static uint32_t lastActivityMs  = 0;
static uint32_t lastLedToggle   = 0;
static bool     ledState        = false;

static void resetIdleTimer() {
    lastActivityMs = millis();
}

// ----------------------------------------------------------------------------
// Button callback: send HID key press/release
// ----------------------------------------------------------------------------
static void onButton(uint8_t index, ButtonAction action) {
    resetIdleTimer();

    // Serial protocol: send binary event to host
    proto_send_btn_event(index, action == BTN_PRESSED ? ACTION_PRESSED : ACTION_RELEASED);

    // BLE HID: send key if connected and no serial host
    if (!proto_host_connected() && bleKeyboard.isConnected()) {
        uint8_t key = BUTTON_KEYS[index];
        if (action == BTN_PRESSED) {
            bleKeyboard.press(key);
        } else {
            bleKeyboard.release(key);
        }
    }
}

// ----------------------------------------------------------------------------
// Encoder button press/release callback (button index 9)
// ----------------------------------------------------------------------------
static void onEncoderButton(bool pressed) {
    resetIdleTimer();

    // Serial protocol: encoder button = index 9
    proto_send_btn_event(NUM_BUTTONS, pressed ? ACTION_PRESSED : ACTION_RELEASED);

    // BLE HID: no individual key for encoder press/release (handled by multi-click)
}

// ----------------------------------------------------------------------------
// Encoder rotation callback: volume up/down
// ----------------------------------------------------------------------------
static void onRotation(int direction) {
    resetIdleTimer();

    // Serial protocol
    proto_send_knob_rotate(direction);

    // BLE HID fallback
    if (!proto_host_connected() && bleKeyboard.isConnected()) {
        if (direction > 0) {
            bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
        } else {
            bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
        }
    }
}

// ----------------------------------------------------------------------------
// Encoder button multi-click callback: media controls
// ----------------------------------------------------------------------------
static void onEncoderClick(uint8_t clickCount) {
    resetIdleTimer();

    // Serial protocol
    proto_send_knob_click(clickCount);

    // BLE HID fallback
    if (!proto_host_connected() && bleKeyboard.isConnected()) {
        switch (clickCount) {
            case 1:
                bleKeyboard.write(KEY_MEDIA_PLAY_PAUSE);
                break;
            case 2:
                bleKeyboard.write(KEY_MEDIA_NEXT_TRACK);
                break;
            case 3:
                bleKeyboard.write(KEY_MEDIA_PREVIOUS_TRACK);
                break;
        }
    }
}

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
static void checkIdleSleep() {
    if (bleKeyboard.isConnected()) return;

    uint32_t idleMs = millis() - lastActivityMs;
    if (idleMs < IDLE_TIMEOUT_MS) return;

#ifndef DISABLE_KNOB
    uint64_t gpioMask = buttons_get_wakeup_mask() | knob_get_wakeup_mask();
#else
    uint64_t gpioMask = buttons_get_wakeup_mask();
#endif
    if (gpioMask) {
        esp_sleep_enable_ext1_wakeup(gpioMask, ESP_EXT1_WAKEUP_ALL_LOW);
    }
    esp_sleep_enable_timer_wakeup(5000000);  // 5s safety net
    esp_light_sleep_start();

    // Woke up
    resetIdleTimer();
}

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(115200);

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    proto_init();
    bleKeyboard.begin();

    buttons_init();
#ifndef DISABLE_KNOB
    knob_init();
#endif

    buttons_set_callback(onButton);
#ifndef DISABLE_KNOB
    knob_set_rotation_callback(onRotation);
    knob_set_button_callback(onEncoderButton);
    knob_set_click_callback(onEncoderClick);
#endif

    lastActivityMs = millis();
}

// ============================================================================
// Main Loop
// ============================================================================
void loop() {
    uint32_t now = millis();

    if (now - lastButtonScan >= BUTTON_SCAN_INTERVAL) {
        lastButtonScan = now;
        buttons_scan();
#ifndef DISABLE_KNOB
        knob_scan_button();
#endif
    }

    if (now - lastEncoderScan >= ENCODER_SCAN_INTERVAL) {
        lastEncoderScan = now;
#ifndef DISABLE_KNOB
        knob_scan_rotation();
#endif
    }

    // Process incoming serial commands
    proto_process();

    // Status LED: aceso fixo = conectado, piscando = desconectado
    if (bleKeyboard.isConnected()) {
        if (!ledState) {
            ledState = true;
            digitalWrite(STATUS_LED_PIN, HIGH);
        }
    } else {
        if (now - lastLedToggle >= LED_BLINK_INTERVAL) {
            lastLedToggle = now;
            ledState = !ledState;
            digitalWrite(STATUS_LED_PIN, ledState ? HIGH : LOW);
        }
    }

    if (now - lastSleepCheck >= SLEEP_CHECK_INTERVAL) {
        lastSleepCheck = now;
        checkIdleSleep();
    }

    delay(1);  // Yield to FreeRTOS / watchdog
}
