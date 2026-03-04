// ============================================================================
// LEDs Module - Implementation
// ============================================================================

#include "leds.h"
#include <ArduinoJson.h>

#if LED_ENABLED
#include <FastLED.h>

static CRGB     strip[NUM_LEDS];
static uint32_t targetColor[NUM_LEDS];   // Persistent color set by host or default
static uint32_t flashUntil[NUM_LEDS];    // Millis timestamp for press-flash end

// Resolve a color name or hex string to a 0xRRGGBB value.
static uint32_t lookupColor(const char* name) {
    if (!name) return LED_COLOR_OFF;

    // Try hex format: "#FF0000" or "FF0000"
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
#endif  // LED_ENABLED

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void leds_init() {
#if LED_ENABLED
    FastLED.addLeds<LED_TYPE, LED_DATA_PIN, LED_COLOR_ORDER>(strip, NUM_LEDS);
    FastLED.setBrightness(LED_BRIGHTNESS);
    for (int i = 0; i < NUM_LEDS; i++) {
        targetColor[i] = LED_COLOR_IDLE;
        flashUntil[i]  = 0;
        strip[i] = CRGB(LED_COLOR_IDLE);
    }
    FastLED.show();
#endif
}

void leds_update() {
#if LED_ENABLED
    uint32_t now = millis();
    for (int i = 0; i < NUM_LEDS; i++) {
        if (flashUntil[i] > now) {
            strip[i] = CRGB(LED_COLOR_PRESSED);
        } else {
            strip[i] = CRGB(targetColor[i]);
        }
    }
    FastLED.show();
#endif
}

void leds_flash_button(int btnIndex) {
#if LED_ENABLED
    if (btnIndex >= 0 && btnIndex < NUM_LEDS) {
        flashUntil[btnIndex] = millis() + 150;
    }
#endif
}

const char* leds_handle_command(const char* json) {
#if LED_ENABLED
    StaticJsonDocument<JSON_RX_BUFFER_SIZE> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) return nullptr;

    // Per-button LED color: {"btn": 3, "led": "red"}
    if (doc.containsKey("btn") && doc.containsKey("led")) {
        int idx = doc["btn"].as<int>();
        const char* colorName = doc["led"].as<const char*>();
        if (idx >= 0 && idx < NUM_LEDS && colorName) {
            targetColor[idx] = lookupColor(colorName);
        }
        return "led";
    }

    // Global brightness: {"brightness": 80}
    if (doc.containsKey("brightness")) {
        int b = doc["brightness"].as<int>();
        FastLED.setBrightness(constrain(b, 0, 255));
        return "brightness";
    }

    // All LEDs: {"all_leds": "off"}
    if (doc.containsKey("all_leds")) {
        const char* colorName = doc["all_leds"].as<const char*>();
        if (colorName) {
            uint32_t color = lookupColor(colorName);
            for (int i = 0; i < NUM_LEDS; i++) {
                targetColor[i] = color;
            }
        }
        return "all_leds";
    }
#endif
    return nullptr;
}

void leds_sleep() {
#if LED_ENABLED
    FastLED.setBrightness(0);
    FastLED.show();
#endif
}

void leds_wake() {
#if LED_ENABLED
    FastLED.setBrightness(LED_BRIGHTNESS);
#endif
}
