# Open StreamDeck — Communication Protocol

## Overview

The Open StreamDeck communicates via **newline-delimited JSON** (`\n`).
Each message is a complete JSON line. Two transport channels are available:

| Transport           | Details                                                      |
|---------------------|--------------------------------------------------------------|
| **USB Serial**      | 115200 baud, 8N1. Connect via USB cable to the ESP32.        |
| **Bluetooth (SPP)** | Appears as "OpenStreamDeck" in Windows. After pairing, the OS creates a virtual COM port — use the same baud rate (115200). |

Both channels are equivalent: same events, same commands, same responses.

---

## Events: Deck → Host

The deck automatically sends events when the user interacts with the controls.

### Buttons

12 buttons (indices 0–11). Three action types:

```json
{"btn": 0, "action": "pressed"}
{"btn": 0, "action": "released"}
{"btn": 0, "action": "long_pressed"}
```

| Field    | Type   | Description                                        |
|----------|--------|----------------------------------------------------|
| `btn`    | int    | Button index (0–11)                                |
| `action` | string | `"pressed"`, `"released"`, or `"long_pressed"`     |

- **pressed**: fires the moment the button is pressed (after 30ms debounce)
- **released**: fires when the button is released
- **long_pressed**: fires if the button is held for ≥ 600ms

Typical short-click sequence:
```
{"btn":3,"action":"pressed"}
{"btn":3,"action":"released"}
```

Typical long-click sequence:
```
{"btn":3,"action":"pressed"}
{"btn":3,"action":"long_pressed"}
{"btn":3,"action":"released"}
```

### Knob (Rotary Encoder)

Each knob has a string ID defined in the configuration (default: `"volume"`).

**Rotation:**
```json
{"knob": "volume", "direction": "up"}
{"knob": "volume", "direction": "down"}
```

**Knob button (pressing the encoder):**
```json
{"knob": "volume", "pressed": true}
{"knob": "volume", "pressed": false}
```

| Field       | Type    | Description                               |
|-------------|---------|-------------------------------------------|
| `knob`      | string  | Knob ID (e.g. `"volume"`)                 |
| `direction` | string  | `"up"` or `"down"` (rotation events only) |
| `pressed`   | boolean | `true`/`false` (button events only)       |

### Slider (Potentiometer / Fader)

Each slider has a string ID (default: `"fader1"`). Sends raw 12-bit ADC values.

```json
{"slider": "fader1", "value": 2048}
```

| Field    | Type   | Description                         |
|----------|--------|-------------------------------------|
| `slider` | string | Slider ID (e.g. `"fader1"`)         |
| `value`  | int    | ADC value from 0 to 4095            |

The slider only sends events when the change exceeds the configured threshold (default: 20 ADC units) to avoid noise.

### Heartbeat

The deck sends a heartbeat every 5 seconds:

```json
{"status": "alive", "uptime": 120, "bt": true}
```

| Field    | Type    | Description                                    |
|----------|---------|------------------------------------------------|
| `status` | string  | Always `"alive"`                               |
| `uptime` | int     | Seconds since boot                             |
| `bt`     | boolean | `true` if a Bluetooth client is connected      |

Use the heartbeat to detect whether the deck is online. If no heartbeat is received for more than 10 seconds, consider the device disconnected.

---

## Commands: Host → Deck

The host sends JSON commands to control the deck. Each command receives an ACK/NACK response.

### Single LED Control

Set the color of the LED associated with a button:

```json
{"btn": 3, "led": "red"}
```

Named colors:

| Name       | Hex       |
|------------|-----------|
| `red`      | `#FF0000` |
| `green`    | `#00FF00` |
| `blue`     | `#0000FF` |
| `yellow`   | `#FFFF00` |
| `cyan`     | `#00FFFF` |
| `magenta`  | `#FF00FF` |
| `white`    | `#FFFFFF` |
| `orange`   | `#FF8000` |
| `purple`   | `#8000FF` |
| `off`      | `#000000` |

Hex color (with or without `#`):

```json
{"btn": 5, "led": "#FF8000"}
{"btn": 5, "led": "FF8000"}
```

### All LEDs Control

Set the same color for all LEDs at once:

```json
{"all_leds": "cyan"}
{"all_leds": "#FF0000"}
{"all_leds": "off"}
```

### Global Brightness

Set brightness for all LEDs (0–255):

```json
{"brightness": 80}
```

---

## Responses (ACK/NACK)

Every command receives a response:

**Success:**
```json
{"ack": true, "cmd": "led"}
{"ack": true, "cmd": "brightness"}
{"ack": true, "cmd": "all_leds"}
```

**Error — unknown command:**
```json
{"ack": false, "error": "unknown_command"}
```

**Error — invalid JSON:**
```json
{"ack": false, "error": "parse_error"}
```

---

## Message Summary

### Deck → Host (events)

| Event          | Example                                          |
|----------------|--------------------------------------------------|
| Button pressed | `{"btn":0,"action":"pressed"}`                   |
| Button released| `{"btn":0,"action":"released"}`                  |
| Long press     | `{"btn":0,"action":"long_pressed"}`              |
| Knob rotation  | `{"knob":"volume","direction":"up"}`             |
| Knob button    | `{"knob":"volume","pressed":true}`               |
| Slider         | `{"slider":"fader1","value":2048}`               |
| Heartbeat      | `{"status":"alive","uptime":120,"bt":true}`      |

### Host → Deck (commands)

| Command        | Example                        |
|----------------|--------------------------------|
| Single LED     | `{"btn":3,"led":"red"}`        |
| All LEDs       | `{"all_leds":"off"}`           |
| Brightness     | `{"brightness":80}`            |

---

## Full Session Example

```
# Deck connects, host starts receiving heartbeats:
← {"status":"alive","uptime":0,"bt":true}

# Host sets button colors:
→ {"btn":0,"led":"green"}
← {"ack":true,"cmd":"led"}
→ {"btn":1,"led":"red"}
← {"ack":true,"cmd":"led"}

# User presses button 0:
← {"btn":0,"action":"pressed"}
← {"btn":0,"action":"released"}

# User rotates the knob:
← {"knob":"volume","direction":"up"}
← {"knob":"volume","direction":"up"}
← {"knob":"volume","direction":"down"}

# User moves the slider:
← {"slider":"fader1","value":1024}
← {"slider":"fader1","value":2048}

# Host turns off all LEDs:
→ {"all_leds":"off"}
← {"ack":true,"cmd":"all_leds"}

# Host sends invalid JSON:
→ not_json
← {"ack":false,"error":"parse_error"}
```

Legend: `→` = Host to Deck | `←` = Deck to Host

---

## Default Hardware Configuration

| Component       | Count | GPIOs                                        |
|-----------------|-------|----------------------------------------------|
| Buttons         | 12    | 4, 5, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23 |
| Knob (encoder)  | 1     | CLK=32, DT=33, SW=25                        |
| Slider (fader)  | 1     | ADC=34                                       |
| LEDs (WS2812B)  | 12    | Data=26                                      |

---

## App Implementation Notes

1. **Connection**: Open the COM port (USB or Bluetooth SPP) at 115200 baud, 8N1.
2. **Reading**: Read line by line (`\n` as delimiter). Each line is a complete JSON object.
3. **Writing**: Send JSON commands terminated with `\n`.
4. **Heartbeat**: Use as a connection indicator. Suggested timeout: 10s without heartbeat = disconnected.
5. **Threading**: A separate thread for continuous serial reading with a queue/callback for event processing is recommended.
6. **LEDs are optional**: If `LED_ENABLED=0` in the firmware, LED commands will be responded with `unknown_command`.
