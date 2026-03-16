// ============================================================================
// Open StreamDeck Firmware - BLE HID Macro Keyboard
// ESP32 DOIT DevKit V1 | 9 Buttons + Rotary Encoder (Volume + Media)
// ============================================================================
//
// Buttons:  Send F13-F21 key press/release via BLE HID
// Encoder:  Rotate = Volume Up/Down
// Enc Btn:  1 click = Play/Pause, 2 clicks = Next Track, 3 clicks = Prev Track
// ============================================================================

#include <Arduino.h>
#include <BleKeyboard.h>
#include "config.h"
#include "buttons.h"
#include "knob.h"

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
    if (!bleKeyboard.isConnected()) return;
    resetIdleTimer();

    uint8_t key = BUTTON_KEYS[index];
    if (action == BTN_PRESSED) {
        bleKeyboard.press(key);
    } else {
        bleKeyboard.release(key);
    }
}

// ----------------------------------------------------------------------------
// Encoder rotation callback: volume up/down
// ----------------------------------------------------------------------------
static void onRotation(int direction) {
    if (!bleKeyboard.isConnected()) return;
    resetIdleTimer();

    if (direction > 0) {
        bleKeyboard.write(KEY_MEDIA_VOLUME_UP);
    } else {
        bleKeyboard.write(KEY_MEDIA_VOLUME_DOWN);
    }
}

// ----------------------------------------------------------------------------
// Encoder button multi-click callback: media controls
// ----------------------------------------------------------------------------
static void onEncoderClick(uint8_t clickCount) {
    if (!bleKeyboard.isConnected()) return;
    resetIdleTimer();

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

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
static void checkIdleSleep() {
    if (bleKeyboard.isConnected()) return;

    uint32_t idleMs = millis() - lastActivityMs;
    if (idleMs < IDLE_TIMEOUT_MS) return;

    uint64_t gpioMask = buttons_get_wakeup_mask() | knob_get_wakeup_mask();
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
    Serial.println("OpenDeck BLE HID starting...");

    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, LOW);

    bleKeyboard.begin();

    buttons_init();
    knob_init();

    buttons_set_callback(onButton);
    knob_set_rotation_callback(onRotation);
    knob_set_click_callback(onEncoderClick);

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
        knob_scan_button();
    }

    if (now - lastEncoderScan >= ENCODER_SCAN_INTERVAL) {
        lastEncoderScan = now;
        knob_scan_rotation();
    }

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
