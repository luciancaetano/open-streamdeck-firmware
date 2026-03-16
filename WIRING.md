# OpenDeck - Wiring Guide

## Components

- 1x ESP32 DOIT DevKit V1
- 9x Momentary push buttons
- 1x EC11 rotary encoder with built-in push button (5 pins: CLK, DT, SW, GND, GND)

## Overview

All **9 buttons** are wired on the **left side** of the ESP32.
The **rotary encoder** is wired on the **right side**.

Buttons use the ESP32 internal pull-up — each button connects a GPIO pin to GND. No external resistors needed.

## ESP32 Pinout Diagram

```
               ┌──────────────┐
          3V3  │              │ VIN
          GND  │              │ GND
          D15  │              │ D13
          D2   │ (status LED) │ D12
  BTN 0 → D4   │              │ D14
  BTN 1 → RX2  │ (GPIO16)     │ D27
  BTN 2 → TX2  │ (GPIO17)     │ D26
  BTN 3 → D5   │              │ D25 ← ENC SW
  BTN 4 → D18  │              │ D33 ← ENC DT
  BTN 5 → D19  │              │ D32 ← ENC CLK
  BTN 6 → D21  │              │ D35
          RX0  │ (GPIO3)      │ D34
          TX0  │ (GPIO1)      │ VN
  BTN 7 → D22  │              │ VP
  BTN 8 → D23  │              │ EN
               └──────┬┬──────┘
                      ││ USB
```

## Button Layout (3x3)

```
┌───────┬───────┬───────┐
│ BTN 0 │ BTN 1 │ BTN 2 │  F13  F14  F15
├───────┼───────┼───────┤
│ BTN 3 │ BTN 4 │ BTN 5 │  F16  F17  F18
├───────┼───────┼───────┤
│ BTN 6 │ BTN 7 │ BTN 8 │  F19  F20  F21
└───────┴───────┴───────┘
         [KNOB]
```

## Button Wiring

Each button has 2 terminals. One goes to the GPIO pin, the other goes to GND.

| Button | ESP32 Pin | GPIO | HID Key |
| ------ | --------- | ---- | ------- |
| BTN 0  | D4        | 4    | F13     |
| BTN 1  | RX2       | 16   | F14     |
| BTN 2  | TX2       | 17   | F15     |
| BTN 3  | D5        | 5    | F16     |
| BTN 4  | D18       | 18   | F17     |
| BTN 5  | D19       | 19   | F18     |
| BTN 6  | D21       | 21   | F19     |
| BTN 7  | D22       | 22   | F20     |
| BTN 8  | D23       | 23   | F21     |

### How to wire each button

```
ESP32 GPIO ──────┤ ├────── ESP32 GND
  (e.g. D4)     button
```

All buttons share the same GND. You can use a common GND wire for all of them.

```
ESP32 GND ──┬──┬──┬──┬──┬──┬──┬──┬──
            │  │  │  │  │  │  │  │  │
           B0 B1 B2 B3 B4 B5 B6 B7 B8
            │  │  │  │  │  │  │  │  │
ESP32 D4 ───┘  │  │  │  │  │  │  │  │
ESP32 RX2 ─────┘  │  │  │  │  │  │  │
ESP32 TX2 ────────┘  │  │  │  │  │  │
ESP32 D5 ────────────┘  │  │  │  │  │
ESP32 D18 ──────────────┘  │  │  │  │
ESP32 D19 ─────────────────┘  │  │  │
ESP32 D21 ────────────────────┘  │  │
ESP32 D22 ───────────────────────┘  │
ESP32 D23 ──────────────────────────┘
```

## Rotary Encoder Wiring

The encoder has 5 pins: 3 for rotation and 2 for the built-in push button.

### Encoder pins (rotation)

| Encoder Pin | ESP32 Pin | GPIO |
| ----------- | --------- | ---- |
| CLK         | D32       | 32   |
| DT          | D33       | 33   |
| GND         | GND       | -    |

### Built-in button pins (click)

| Encoder Pin | ESP32 Pin | GPIO |
| ----------- | --------- | ---- |
| SW          | D25       | 25   |
| GND         | GND       | -    |

### Encoder diagram

```
Encoder (front view)

    ┌─────────┐
    │  ╭───╮  │
    │  │   │  │  ← rotary shaft
    │  ╰───╯  │
    └┬──┬──┬──┘
     │  │  │      ← 3 encoder pins
    CLK GND DT
     │   │   │
     │   │   └──── ESP32 D33 (GPIO33)
     │   └──────── ESP32 GND
     └──────────── ESP32 D32 (GPIO32)

      ┌┤  ├┐      ← 2 button pins
      SW   GND
      │     │
      │     └───── ESP32 GND
      └─────────── ESP32 D25 (GPIO25)
```

## Status LED

The ESP32 onboard LED (GPIO 2) indicates BLE connection status:

- **Solid ON** = connected via Bluetooth
- **Blinking** = waiting for connection

No wiring needed — it's already on the board.

## Tips

- Use a common GND wire to simplify wiring
- Buttons don't need external resistors — the ESP32 uses internal pull-ups
- The encoder also doesn't need resistors — internal pull-ups are enabled
- Check your specific encoder's pin layout — the order may vary between models
