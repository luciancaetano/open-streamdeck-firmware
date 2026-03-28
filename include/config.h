#pragma once
// ============================================================================
// Open StreamDeck - Configuration
// ESP32 DOIT DevKit V1 | BLE HID Macro Keyboard
// 10 Buttons (9 Otemu switches + 1 encoder button) + 1 Rotary Encoder
// ============================================================================

#include <Arduino.h>

// ----------------------------------------------------------------------------
// Device Identity (shown in Bluetooth pairing)
// ----------------------------------------------------------------------------
#define DEVICE_NAME           "OpenDeck"
#define DEVICE_MANUFACTURER   "OpenDeck"
#define FIRMWARE_VERSION      "2.0.0"
#define DEVICE_HARDWARE_ID    "ODK10K1"   // 8-char unique hardware identifier
                                          // Format: ODK + total buttons + K + knob count
                                          // 10 buttons (9 Otemu + 1 encoder btn), 1 knob
                                          // Used for auto-discovery on serial ports

// ----------------------------------------------------------------------------
// Button Configuration (active LOW with internal pull-up)
// ----------------------------------------------------------------------------
#define NUM_BUTTONS           9
#define DEBOUNCE_MS           30

// ESP32 DOIT DevKit V1 - Pin mapping
//
//        ┌──────────────┐
//   3V3  │              │ VIN
//   GND  │              │ GND
//   D15  │              │ D13
//   D2   │ (LED status) │ D12
//   D4   │ ← BTN 0      │ D14
//   RX2  │ ← BTN 1 (16) │ D27
//   TX2  │ ← BTN 2 (17) │ D26
//   D5   │ ← BTN 3      │ D25  ← ENC SW
//   D18  │ ← BTN 4      │ D33  ← ENC DT
//   D19  │ ← BTN 5      │ D32  ← ENC CLK
//   D21  │ ← BTN 6      │ D35  (input-only)
//   RX0  │(GPIO3)       │ D34  (input-only)
//   TX0  │(GPIO1)       │ VN   (GPIO39, input-only)
//   D22  │ ← BTN 7      │ VP   (GPIO36, input-only)
//   D23  │ ← BTN 8      │ EN
//        └──────┬┬──────┘
//               ││ USB
//
// GPIO pins for 9 buttons (lado esquerdo, em ordem física de cima para baixo)
// Layout 3x3 (esquerda→direita, cima→baixo):
//   BTN 0  BTN 1  BTN 2
//   BTN 3  BTN 4  BTN 5
//   BTN 6  BTN 7  BTN 8
static const uint8_t BUTTON_PINS[NUM_BUTTONS] = {
    4,    // BTN 0  -> pino D4
    16,   // BTN 1  -> pino RX2 (GPIO16)
    17,   // BTN 2  -> pino TX2 (GPIO17)
    5,    // BTN 3  -> pino D5
    18,   // BTN 4  -> pino D18
    19,   // BTN 5  -> pino D19
    21,   // BTN 6  -> pino D21
    22,   // BTN 7  -> pino D22
    23,   // BTN 8  -> pino D23
};

// HID key codes for each button (F13-F21 by default)
// F13=0xF0, F14=0xF1, ..., F21=0xF8 (ESP32-BLE-Keyboard key codes)
static const uint8_t BUTTON_KEYS[NUM_BUTTONS] = {
    0xF0,  // BTN 0 -> F13
    0xF1,  // BTN 1 -> F14
    0xF2,  // BTN 2 -> F15
    0xF3,  // BTN 3 -> F16
    0xF4,  // BTN 4 -> F17
    0xF5,  // BTN 5 -> F18
    0xF6,  // BTN 6 -> F19
    0xF7,  // BTN 7 -> F20
    0xF8,  // BTN 8 -> F21
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
// Knob disable option
// ----------------------------------------------------------------------------
#define DISABLE_KNOB        // Uncomment to disable rotary encoder and encoder button support

// ----------------------------------------------------------------------------
// Multi-Click Detection (encoder button)
// ----------------------------------------------------------------------------
#define MULTI_CLICK_TIMEOUT   400     // ms to wait for additional clicks

// ----------------------------------------------------------------------------
// Status LED (onboard LED - GPIO2)
// ----------------------------------------------------------------------------
#define STATUS_LED_PIN        2       // LED embutido da ESP32 DevKit V1
#define LED_BLINK_INTERVAL    500     // Piscar a cada 500ms quando desconectado

// ----------------------------------------------------------------------------
// Scan & Loop Timing
// ----------------------------------------------------------------------------
#define BUTTON_SCAN_INTERVAL  5       // Scan buttons every 5ms
#define ENCODER_SCAN_INTERVAL 2       // Read encoder every 2ms

// ----------------------------------------------------------------------------
// Serial Protocol
// ----------------------------------------------------------------------------
#define PROTO_HEARTBEAT_INTERVAL_MS  3000    // Send heartbeat every 3s
#define PROTO_HOST_TIMEOUT_MS        10000   // Host considered gone after 10s

// ----------------------------------------------------------------------------
// Power Management
// ----------------------------------------------------------------------------
#define IDLE_TIMEOUT_MS       300000  // 5 min idle before light sleep
#define SLEEP_CHECK_INTERVAL  1000

// ============================================================================
// Alimentação por Bateria - ESP32 DOIT DevKit V1
// ============================================================================
//
// CONSUMO DE CORRENTE (típico):
//   BLE ativo + conectado:  ~80-130 mA
//   BLE advertising (idle): ~100-150 mA
//   Light sleep:            ~0.8 mA
//   Deep sleep:             ~10 µA
//
// ────────────────────────────────────────────────────────────────────────────
// OPÇÃO 1: Pino VIN (recomendado para simplicidade)
// ────────────────────────────────────────────────────────────────────────────
//   Tensão: 5V (mín 4.5V, máx 12V)
//   O pino VIN alimenta o regulador AMS1117-3.3V onboard.
//   Dropout ~1.2V, por isso precisa de no mínimo 4.5V.
//
//   Conexão:
//     BAT+ ──→ Boost converter 5V ──→ VIN
//     BAT- ──→ GND
//
//   Baterias compatíveis:
//     - 1x LiPo 3.7V + módulo boost MT3608 (3.7V → 5V)
//     - 1x 18650 3.7V + módulo boost MT3608 (3.7V → 5V)
//     - Power bank USB 5V (via pino VIN ou cabo USB)
//     - 4x pilhas AA (6V) direto no VIN
//
// ────────────────────────────────────────────────────────────────────────────
// OPÇÃO 2: Pino 3V3 (mais eficiente, sem desperdício no regulador)
// ────────────────────────────────────────────────────────────────────────────
//   Tensão: 3.3V regulado (mín 3.0V, máx 3.6V)
//   Conecta direto no barramento 3.3V, bypassa o regulador onboard.
//   ⚠ NÃO conectar LiPo direto (4.2V carregada pode danificar o ESP32!)
//
//   Conexão:
//     BAT+ ──→ LDO 3.3V (ex: AMS1117, HT7333, MCP1700) ──→ 3V3
//     BAT- ──→ GND
//
//   Baterias compatíveis:
//     - 1x LiPo 3.7V + LDO 3.3V (ex: HT7333, baixo dropout)
//     - 1x 18650 3.7V + LDO 3.3V
//     - 2x pilhas AA (3V) + LDO 3.3V (margem apertada)
//
// ────────────────────────────────────────────────────────────────────────────
// OPÇÃO 3: USB (mais simples de todas)
// ────────────────────────────────────────────────────────────────────────────
//   Basta conectar um power bank USB no conector micro-USB da placa.
//   Tensão: 5V via USB. Nenhuma modificação necessária.
//
// ────────────────────────────────────────────────────────────────────────────
// DIAGRAMA DE CONEXÃO (Opção 1 - Boost 5V):
// ────────────────────────────────────────────────────────────────────────────
//
//   ┌─────────┐     ┌────────────┐     ┌──────────────┐
//   │  LiPo   │     │  MT3608    │     │  ESP32       │
//   │  3.7V   ├──+──┤ IN+   OUT+ ├─────┤ VIN          │
//   │         │  │  │            │     │              │
//   │  GND  ──├──┴──┤ IN-   OUT- ├─────┤ GND          │
//   └─────────┘     └────────────┘     └──────────────┘
//                    (ajustar p/ 5V)
//
// ────────────────────────────────────────────────────────────────────────────
// ESTIMATIVA DE DURAÇÃO:
// ────────────────────────────────────────────────────────────────────────────
//   Bateria 1000mAh LiPo, uso ativo ~100mA:
//     ~8-10 horas de uso contínuo
//     (considerando ~80% eficiência do boost/LDO)
//
//   Bateria 18650 2600mAh, uso ativo ~100mA:
//     ~20-22 horas de uso contínuo
//
//   Com light sleep (IDLE_TIMEOUT_MS), duração aumenta significativamente
//   quando o dispositivo não está sendo usado ativamente.
// ============================================================================
