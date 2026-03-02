// ============================================================================
// Open StreamDeck Firmware
// ESP32 DOIT DevKit V1 | 12 Buttons + Rotary Encoder + BLE/USB Serial
// ============================================================================

#include <Arduino.h>
#include "config.h"
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <ESP32Encoder.h>

#if LED_ENABLED
#include <FastLED.h>
#endif

// ============================================================================
// Forward Declarations
// ============================================================================
static void initButtons();
static void initEncoder();
static void initBLE();
static void initLEDs();
static void scanButtons();
static void scanEncoder();
static void processSerialRx();
static void processBleRx();
static void handleCommand(const char* json);
static void sendEvent(const char* json);
static void updateLEDs();
static void checkIdleSleep();
static void resetIdleTimer();
static uint32_t lookupColor(const char* name);

// ============================================================================
// Global State
// ============================================================================

// -- BLE --------------------------------------------------------------------
static NimBLEServer*         bleServer         = nullptr;
static NimBLECharacteristic* bleTxChar         = nullptr;
static NimBLECharacteristic* bleRxChar         = nullptr;
static bool                  bleConnected      = false;
static String                bleRxBuffer;

// -- Encoder ----------------------------------------------------------------
static ESP32Encoder encoder;
static int64_t     lastEncoderCount = 0;

// -- Button State -----------------------------------------------------------
struct ButtonState {
    bool     pressed;           // Current debounced state
    bool     lastRaw;           // Last raw reading
    uint32_t lastChangeMs;      // Timestamp of last raw change (for debounce)
    uint32_t pressStartMs;      // When the button was first pressed
    bool     longFired;         // Whether long-press event was already sent
};
static ButtonState buttons[NUM_BUTTONS];
static ButtonState encoderBtn;

// -- LEDs -------------------------------------------------------------------
#if LED_ENABLED
static CRGB leds[NUM_LEDS];
static uint32_t ledTargetColor[NUM_LEDS];  // Persistent color set by host or default
static uint32_t ledFlashUntil[NUM_LEDS];   // Millis timestamp for press-flash end
#endif

// -- Timing -----------------------------------------------------------------
static uint32_t lastButtonScan   = 0;
static uint32_t lastEncoderScan  = 0;
static uint32_t lastLedUpdate    = 0;
static uint32_t lastSerialRx     = 0;
static uint32_t lastSleepCheck   = 0;
static uint32_t lastActivityMs   = 0;

// -- Serial RX buffer -------------------------------------------------------
static char usbRxBuf[SERIAL_RX_LINE_MAX];
static size_t usbRxLen = 0;

// ============================================================================
// BLE Callbacks
// ============================================================================
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* server) override {
        bleConnected = true;
        resetIdleTimer();
    }
    void onDisconnect(NimBLEServer* server) override {
        bleConnected = false;
        NimBLEDevice::startAdvertising();
    }
};

class RxCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* characteristic) override {
        std::string val = characteristic->getValue();
        bleRxBuffer += val.c_str();
        resetIdleTimer();
    }
};

// ============================================================================
// Setup
// ============================================================================
void setup() {
    Serial.begin(USB_SERIAL_BAUD);
    initButtons();
    initEncoder();
    initLEDs();
    initBLE();

    lastActivityMs = millis();
}

// ============================================================================
// Main Loop - Non-blocking, timer-gated
// ============================================================================
void loop() {
    uint32_t now = millis();

    // Scan buttons at fixed interval
    if (now - lastButtonScan >= BUTTON_SCAN_INTERVAL) {
        lastButtonScan = now;
        scanButtons();
    }

    // Scan encoder at fixed interval
    if (now - lastEncoderScan >= ENCODER_SCAN_INTERVAL) {
        lastEncoderScan = now;
        scanEncoder();
    }

    // Process incoming serial data
    if (now - lastSerialRx >= SERIAL_RX_INTERVAL) {
        lastSerialRx = now;
        processSerialRx();
        processBleRx();
    }

    // Update LEDs
#if LED_ENABLED
    if (now - lastLedUpdate >= LED_UPDATE_INTERVAL) {
        lastLedUpdate = now;
        updateLEDs();
    }
#endif

    // Power management: check for idle sleep
    if (now - lastSleepCheck >= SLEEP_CHECK_INTERVAL) {
        lastSleepCheck = now;
        checkIdleSleep();
    }

    // Yield to FreeRTOS / watchdog between cycles
    delay(1);
}

// ============================================================================
// Initialization
// ============================================================================

static void initButtons() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        buttons[i] = { false, true, 0, 0, false };
    }
    // Encoder push button
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    encoderBtn = { false, true, 0, 0, false };
}

static void initEncoder() {
    ESP32Encoder::useInternalWeakPullResistors = UP;
    encoder.attachSingleEdge(ENCODER_PIN_A, ENCODER_PIN_B);
    encoder.setCount(0);
    lastEncoderCount = 0;
}

static void initBLE() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);  // Moderate TX power

    bleServer = NimBLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    NimBLEService* service = bleServer->createService(BLE_SERVICE_UUID);

    bleTxChar = service->createCharacteristic(
        BLE_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    bleRxChar = service->createCharacteristic(
        BLE_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    bleRxChar->setCallbacks(new RxCallbacks());

    service->start();

    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(BLE_SERVICE_UUID);
    advertising->setMinInterval(BLE_ADV_INTERVAL_MIN);
    advertising->setMaxInterval(BLE_ADV_INTERVAL_MAX);
    advertising->start();
}

static void initLEDs() {
#if LED_ENABLED
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    for (int i = 0; i < NUM_LEDS; i++) {
        ledTargetColor[i] = LED_COLOR_IDLE;
        ledFlashUntil[i]  = 0;
        leds[i] = CRGB(LED_COLOR_IDLE);
    }
    FastLED.show();
#endif
}

// ============================================================================
// Button Scanning with Debounce + Long-Press Detection
// ============================================================================

static void scanButtons() {
    uint32_t now = millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool raw = !digitalRead(BUTTON_PINS[i]);  // Active LOW

        // Debounce: only accept change after stable for DEBOUNCE_MS
        if (raw != buttons[i].lastRaw) {
            buttons[i].lastRaw      = raw;
            buttons[i].lastChangeMs = now;
        }

        if ((now - buttons[i].lastChangeMs) >= DEBOUNCE_MS) {
            if (raw && !buttons[i].pressed) {
                // Rising edge: button just pressed
                buttons[i].pressed     = true;
                buttons[i].pressStartMs = now;
                buttons[i].longFired   = false;
                resetIdleTimer();

                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["btn"]    = i;
                doc["action"] = "pressed";
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                sendEvent(buf);

#if LED_ENABLED
                if (i < NUM_LEDS) {
                    ledFlashUntil[i] = now + 150;
                }
#endif
            }
            else if (raw && buttons[i].pressed && !buttons[i].longFired) {
                // Check for long press
                if ((now - buttons[i].pressStartMs) >= LONG_PRESS_MS) {
                    buttons[i].longFired = true;

                    StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                    doc["btn"]    = i;
                    doc["action"] = "long_pressed";
                    char buf[JSON_TX_BUFFER_SIZE];
                    serializeJson(doc, buf);
                    sendEvent(buf);
                }
            }
            else if (!raw && buttons[i].pressed) {
                // Falling edge: button released
                buttons[i].pressed = false;

                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["btn"]    = i;
                doc["action"] = "released";
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                sendEvent(buf);
            }
        }
    }

    // Encoder push button (same debounce logic)
    {
        bool raw = !digitalRead(ENCODER_BTN_PIN);
        if (raw != encoderBtn.lastRaw) {
            encoderBtn.lastRaw      = raw;
            encoderBtn.lastChangeMs = now;
        }
        if ((now - encoderBtn.lastChangeMs) >= ENCODER_DEBOUNCE_MS) {
            if (raw && !encoderBtn.pressed) {
                encoderBtn.pressed = true;
                resetIdleTimer();

                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["knob"]    = "volume";
                doc["pressed"] = true;
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                sendEvent(buf);
            }
            else if (!raw && encoderBtn.pressed) {
                encoderBtn.pressed = false;

                StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
                doc["knob"]    = "volume";
                doc["pressed"] = false;
                char buf[JSON_TX_BUFFER_SIZE];
                serializeJson(doc, buf);
                sendEvent(buf);
            }
        }
    }
}

// ============================================================================
// Rotary Encoder Scanning (hardware-counted via ESP32Encoder interrupts)
// ============================================================================

static void scanEncoder() {
    int64_t count = encoder.getCount();
    int64_t delta = count - lastEncoderCount;

    if (abs((int)delta) >= ENCODER_THRESHOLD) {
        lastEncoderCount = count;
        resetIdleTimer();

        const char* dir = (delta > 0) ? "up" : "down";

        StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
        doc["knob"]      = "volume";
        doc["direction"] = dir;
        char buf[JSON_TX_BUFFER_SIZE];
        serializeJson(doc, buf);
        sendEvent(buf);
    }
}

// ============================================================================
// Dual Serial Output (BLE preferred, USB fallback)
// ============================================================================

static void sendEvent(const char* json) {
    // Always send over USB for debugging
    Serial.println(json);

    // Send over BLE if connected
    if (bleConnected && bleTxChar) {
        bleTxChar->setValue((const uint8_t*)json, strlen(json));
        bleTxChar->notify();
    }
}

// ============================================================================
// Incoming Command Processing
// ============================================================================

static void processSerialRx() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (usbRxLen > 0) {
                usbRxBuf[usbRxLen] = '\0';
                handleCommand(usbRxBuf);
                usbRxLen = 0;
            }
        } else if (usbRxLen < SERIAL_RX_LINE_MAX - 1) {
            usbRxBuf[usbRxLen++] = c;
        }
    }
}

static void processBleRx() {
    if (bleRxBuffer.length() == 0) return;

    // Process complete lines from BLE buffer
    int nlPos;
    while ((nlPos = bleRxBuffer.indexOf('\n')) >= 0) {
        String line = bleRxBuffer.substring(0, nlPos);
        line.trim();
        if (line.length() > 0) {
            handleCommand(line.c_str());
        }
        bleRxBuffer = bleRxBuffer.substring(nlPos + 1);
    }

    // If buffer has no newline but is large, treat whole buffer as a command
    if (bleRxBuffer.length() > SERIAL_RX_LINE_MAX) {
        handleCommand(bleRxBuffer.c_str());
        bleRxBuffer = "";
    }
}

static void handleCommand(const char* json) {
    resetIdleTimer();

    StaticJsonDocument<JSON_RX_BUFFER_SIZE> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        Serial.print("[CMD] JSON parse error: ");
        Serial.println(err.c_str());
        return;
    }

    // LED control: { "btn": 3, "led": "red" }
    if (doc.containsKey("btn") && doc.containsKey("led")) {
        int btnIdx = doc["btn"].as<int>();
        const char* colorName = doc["led"].as<const char*>();

#if LED_ENABLED
        if (btnIdx >= 0 && btnIdx < NUM_LEDS && colorName) {
            uint32_t color = lookupColor(colorName);
            ledTargetColor[btnIdx] = color;
        }
#endif
        return;
    }

    // Brightness control: { "brightness": 80 }
    if (doc.containsKey("brightness")) {
#if LED_ENABLED
        int b = doc["brightness"].as<int>();
        FastLED.setBrightness(constrain(b, 0, 255));
#endif
        return;
    }

    // All-LED control: { "all_leds": "off" }
    if (doc.containsKey("all_leds")) {
#if LED_ENABLED
        const char* colorName = doc["all_leds"].as<const char*>();
        if (colorName) {
            uint32_t color = lookupColor(colorName);
            for (int i = 0; i < NUM_LEDS; i++) {
                ledTargetColor[i] = color;
            }
        }
#endif
        return;
    }

    Serial.print("[CMD] Unknown command: ");
    Serial.println(json);
}

// ============================================================================
// LED Update
// ============================================================================

static void updateLEDs() {
#if LED_ENABLED
    uint32_t now = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        if (ledFlashUntil[i] > now) {
            // Flash white during press feedback
            leds[i] = CRGB(LED_COLOR_PRESSED);
        } else {
            // Show the target color (set by host or default idle)
            leds[i] = CRGB(ledTargetColor[i]);
        }
    }
    FastLED.show();
#endif
}

// ============================================================================
// Color Lookup
// ============================================================================

static uint32_t lookupColor(const char* name) {
    if (!name) return LED_COLOR_OFF;

    // Check if it's a hex string (e.g., "#FF0000" or "FF0000")
    const char* hex = name;
    if (hex[0] == '#') hex++;
    if (strlen(hex) == 6) {
        char* end;
        uint32_t val = strtoul(hex, &end, 16);
        if (*end == '\0') return val;
    }

    // Named color lookup
    for (size_t i = 0; i < COLOR_MAP_SIZE; i++) {
        if (strcasecmp(name, COLOR_MAP[i].name) == 0) {
            return COLOR_MAP[i].color;
        }
    }
    return LED_COLOR_OFF;
}

// ============================================================================
// Power Management
// ============================================================================

static void resetIdleTimer() {
    lastActivityMs = millis();
}

static void checkIdleSleep() {
    // Don't sleep if BLE is connected or USB has data
    if (bleConnected) return;
    if (Serial.available()) return;

    uint32_t idleMs = millis() - lastActivityMs;
    if (idleMs < IDLE_TIMEOUT_MS) return;

    // Dim LEDs before sleeping
#if LED_ENABLED
    FastLED.setBrightness(0);
    FastLED.show();
#endif

    // Configure wakeup sources: any button press
    uint64_t gpioMask = 0;
    for (int i = 0; i < NUM_BUTTONS; i++) {
        if (BUTTON_PINS[i] < 34) {  // Only RTC GPIOs support ext1 wakeup
            gpioMask |= (1ULL << BUTTON_PINS[i]);
        }
    }
    if (ENCODER_BTN_PIN < 34) {
        gpioMask |= (1ULL << ENCODER_BTN_PIN);
    }

    if (gpioMask) {
        esp_sleep_enable_ext1_wakeup(gpioMask, ESP_EXT1_WAKEUP_ALL_LOW);
    }

    // Also wake on timer as a safety net (check BLE reconnect)
    esp_sleep_enable_timer_wakeup(5000000);  // 5 seconds

    esp_light_sleep_start();

    // -- Woke up --
    resetIdleTimer();

#if LED_ENABLED
    FastLED.setBrightness(LED_BRIGHTNESS);
#endif
}
