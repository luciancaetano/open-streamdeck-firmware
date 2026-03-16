# OpenDeck — Serial Protocol Specification

## Overview

Binary protocol over USB Serial (115200 baud, 8N1) designed for **low-latency, low-overhead** communication between the OpenDeck and a host application.

**Design goals:**
- Minimal frame overhead (4 bytes per message)
- Deterministic parsing (state machine, no string parsing)
- CRC-8 integrity check on every frame
- Dual-mode: serial events take priority over BLE HID when a host is connected

---

## Frame Format

Every message (both directions) uses the same binary frame:

```
┌──────┬─────┬──────┬─────────────┬──────┐
│ SYNC │ LEN │ TYPE │ PAYLOAD ... │ CRC8 │
│ 0xAA │ 1B  │ 1B   │ 0-32 bytes  │ 1B   │
└──────┴─────┴──────┴─────────────┴──────┘
```

| Field   | Size    | Description                                          |
|---------|---------|------------------------------------------------------|
| `SYNC`  | 1 byte  | Always `0xAA` — frame synchronization                |
| `LEN`   | 1 byte  | Number of bytes that follow (TYPE + PAYLOAD + CRC)   |
| `TYPE`  | 1 byte  | Message type identifier                              |
| `PAYLOAD` | 0-32 bytes | Message-specific data                           |
| `CRC8`  | 1 byte  | CRC-8/MAXIM of TYPE + PAYLOAD bytes                  |

**LEN** = 1 (TYPE) + N (PAYLOAD) + 1 (CRC) = N + 2

**Minimum frame**: 4 bytes (SYNC + LEN + TYPE + CRC, with 0 payload)
**Maximum frame**: 36 bytes (4 overhead + 32 payload)

---

## CRC-8/MAXIM

- **Polynomial**: 0x31 (x⁸ + x⁵ + x⁴ + 1)
- **Init**: 0x00
- **Input/Output reflect**: No
- **Final XOR**: 0x00
- **Scope**: Computed over TYPE + PAYLOAD bytes (not SYNC or LEN)

### Reference Implementation (C)

```c
uint8_t crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

### Reference Implementation (Python)

```python
def crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x31) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc
```

### Reference Implementation (C#)

```csharp
static byte Crc8(byte[] data, int offset, int length) {
    byte crc = 0x00;
    for (int i = offset; i < offset + length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if ((crc & 0x80) != 0)
                crc = (byte)((crc << 1) ^ 0x31);
            else
                crc = (byte)(crc << 1);
        }
    }
    return crc;
}
```

---

## Message Types

### Deck → Host (0x01 – 0x0F)

| Type   | Name            | Payload                                |
|--------|-----------------|----------------------------------------|
| `0x01` | BTN_EVENT       | `[index:u8] [action:u8]`              |
| `0x02` | KNOB_ROTATE     | `[direction:u8]`                       |
| `0x03` | KNOB_CLICK      | `[click_count:u8]`                     |
| `0x04` | HEARTBEAT       | `[uptime:u32le] [ble:u8]`             |
| `0x05` | DEVICE_INFO     | `[major:u8] [minor:u8] [patch:u8] [buttons:u8] [knobs:u8] [flags:u8]` |
| `0x06` | ACK             | `[cmd_type:u8] [status:u8]`           |
| `0x07` | IDENTIFY        | `[magic:4B] [hardware_id:8B]`          |

### Host → Deck (0x10 – 0x1F)

| Type   | Name            | Payload                                |
|--------|-----------------|----------------------------------------|
| `0x10` | CMD_PING        | _(none)_                               |
| `0x11` | CMD_INFO        | _(none)_                               |
| `0x12` | CMD_SET_BLE     | `[enabled:u8]`                         |
| `0x13` | CMD_SET_KEY     | `[index:u8] [keycode:u8]`             |
| `0x14` | CMD_IDENTIFY    | _(none)_                               |

---

## Message Details

### BTN_EVENT (0x01) — Deck → Host

Sent when a button is pressed or released (after 30ms debounce).

| Byte | Field    | Type | Values                            |
|------|----------|------|-----------------------------------|
| 0    | `index`  | u8   | Button index: `0` – `9`          |
| 1    | `action` | u8   | `0x00` = released, `0x01` = pressed |

Buttons 0–8 are the Otemu switches. Button 9 is the encoder push button.

**Note:** The encoder button (index 9) generates **both** BTN_EVENT (press/release) and KNOB_CLICK (multi-click count after 400ms window). The host can use whichever suits its needs.

**Frame example** — Button 3 pressed:
```
AA 04 01 03 01 xx
│  │  │  │  │  └─ CRC8
│  │  │  │  └──── action: pressed
│  │  │  └─────── index: 3
│  │  └────────── type: BTN_EVENT
│  └───────────── len: 4 (type + 2 payload + crc)
└──────────────── sync
```

**Typical press/release sequence:**
```
AA 04 01 03 01 xx   ← button 3 pressed
AA 04 01 03 00 xx   ← button 3 released
```

### KNOB_ROTATE (0x02) — Deck → Host

Sent on each encoder tick.

| Byte | Field       | Type | Values                          |
|------|-------------|------|---------------------------------|
| 0    | `direction` | u8   | `0x00` = CCW (down), `0x01` = CW (up) |

### KNOB_CLICK (0x03) — Deck → Host

Sent after the multi-click detection window (400ms) closes.

| Byte | Field         | Type | Values                      |
|------|---------------|------|-----------------------------|
| 0    | `click_count` | u8   | `1` = single, `2` = double, `3` = triple |

### HEARTBEAT (0x04) — Deck → Host

Sent automatically every 3 seconds while a host is connected. Also sent as a response to CMD_PING.

| Byte  | Field    | Type   | Description                    |
|-------|----------|--------|--------------------------------|
| 0-3   | `uptime` | u32le  | Seconds since boot (little-endian) |
| 4     | `ble`    | u8     | `0x01` = BLE connected, `0x00` = not |

**Host timeout**: If no heartbeat is received for **10 seconds**, consider the device disconnected.

### DEVICE_INFO (0x05) — Deck → Host

Response to CMD_INFO. Describes hardware capabilities.

| Byte | Field     | Type | Description                     |
|------|-----------|------|---------------------------------|
| 0    | `major`   | u8   | Firmware major version          |
| 1    | `minor`   | u8   | Firmware minor version          |
| 2    | `patch`   | u8   | Firmware patch version          |
| 3    | `buttons` | u8   | Total clickable buttons (incl. encoder btn) |
| 4    | `knobs`   | u8   | Number of rotary encoder axes   |
| 5    | `flags`   | u8   | Capability flags (see below)    |

**Flags byte:**

| Bit | Name         | Description                    |
|-----|--------------|--------------------------------|
| 0   | BLE_CAPABLE  | Device supports Bluetooth HID  |
| 1-7 | _reserved_  | Must be 0                      |

### ACK (0x06) — Deck → Host

Response to commands that modify state.

| Byte | Field      | Type | Description                     |
|------|------------|------|---------------------------------|
| 0    | `cmd_type` | u8   | The command type being acknowledged |
| 1    | `status`   | u8   | `0x00` = OK, `0x01` = unknown command, `0x02` = bad payload |

### IDENTIFY (0x07) — Deck → Host

Response to CMD_IDENTIFY. Used for **auto-discovery** — the host scans COM ports looking for OpenDeck devices.

| Byte  | Field         | Type   | Description                            |
|-------|---------------|--------|----------------------------------------|
| 0     | `magic[0]`    | u8     | `0x0D` — fixed magic byte             |
| 1     | `magic[1]`    | u8     | `0xEC` — fixed magic byte             |
| 2     | `magic[2]`    | u8     | `0x4B` — fixed magic byte ('K')       |
| 3     | `magic[3]`    | u8     | `0x01` — protocol version             |
| 4-11  | `hardware_id` | char[8]| ASCII hardware ID, null-padded         |

**Magic bytes** `0D EC 4B 01` are a fixed signature. If the first 4 bytes of the payload match, the device is confirmed as an OpenDeck.

**Hardware ID** is an 8-character ASCII string that identifies the hardware variant. Default: `"ODK10K1"` (OpenDeck, 10 buttons, 1 knob). Padded with `0x00` if shorter than 8 characters.

**Important:** CMD_IDENTIFY is **passive** — it does NOT activate serial mode. This allows the host to safely scan all COM ports without accidentally disabling BLE HID on the device.

### CMD_PING (0x10) — Host → Deck

Request a heartbeat response. Use this to establish the connection.

**No payload.** The deck responds with a HEARTBEAT message.

**Frame**: `AA 02 10 xx` (4 bytes total)

### CMD_INFO (0x11) — Host → Deck

Request device information.

**No payload.** The deck responds with a DEVICE_INFO message.

**Frame**: `AA 02 11 xx` (4 bytes total)

### CMD_SET_BLE (0x12) — Host → Deck

Enable or disable BLE HID output.

| Byte | Field     | Type | Values                         |
|------|-----------|------|--------------------------------|
| 0    | `enabled` | u8   | `0x00` = disable, `0x01` = enable |

Responds with ACK.

### CMD_SET_KEY (0x13) — Host → Deck

Remap a button to a different HID keycode.

| Byte | Field     | Type | Description                     |
|------|-----------|------|---------------------------------|
| 0    | `index`   | u8   | Button index: `0` – `8`        |
| 1    | `keycode` | u8   | New HID keycode                 |

Responds with ACK.

### CMD_IDENTIFY (0x14) — Host → Deck

Request the hardware identity. Used for **auto-discovery** of OpenDeck devices on serial ports.

**No payload.** The deck responds with an IDENTIFY message.

**Frame**: `AA 02 14 xx` (4 bytes total)

**This command is passive** — it does not mark the host as connected and does not suppress BLE HID. This is critical for safe port scanning: the host can probe every COM port without side effects.

---

## Auto-Discovery Flow

The host application should use CMD_IDENTIFY to find the OpenDeck on startup:

```
For each COM port:
  │
  ├── Open port at 115200 baud, 8N1
  ├── Send CMD_IDENTIFY (AA 02 14 xx)
  ├── Wait up to 200ms for response
  │
  ├── Got IDENTIFY response?
  │   ├── YES: Check magic bytes (0D EC 4B 01)
  │   │   ├── Match: This is an OpenDeck! Read hardware_id.
  │   │   └── No match: Not our device. Close port.
  │   └── NO (timeout): Not our device. Close port.
  │
  └── OpenDeck found → proceed with CMD_PING to activate serial mode
```

**After discovery**, send CMD_PING to activate the serial connection (this is what switches the device from BLE HID to serial mode).

### Python Auto-Discovery Example

```python
import serial
import serial.tools.list_ports

MAGIC = bytes([0x0D, 0xEC, 0x4B, 0x01])

def find_opendeck() -> tuple[str, str] | None:
    """Scan COM ports for an OpenDeck device.
    Returns (port_name, hardware_id) or None."""
    identify_frame = build_frame(0x14)  # CMD_IDENTIFY

    for port_info in serial.tools.list_ports.comports():
        try:
            with serial.Serial(port_info.device, 115200, timeout=0.2) as ser:
                ser.write(identify_frame)
                # Read response (max 16 bytes: AA + LEN + TYPE + 12 payload + CRC)
                response = ser.read(16)
                if len(response) < 16:
                    continue

                # Parse: find sync byte, extract payload
                idx = response.find(b'\xAA')
                if idx < 0 or idx + 16 > len(response):
                    continue

                frame = response[idx:]
                if frame[1] != 14:  # LEN = 12 payload + type + crc
                    continue

                payload = frame[3:15]  # skip SYNC, LEN, TYPE
                if payload[:4] == MAGIC:
                    hw_id = payload[4:12].rstrip(b'\x00').decode('ascii')
                    return (port_info.device, hw_id)
        except (serial.SerialException, OSError):
            continue

    return None
```

### C# Auto-Discovery Example

```csharp
static (string port, string hwId)? FindOpenDeck() {
    byte[] magic = { 0x0D, 0xEC, 0x4B, 0x01 };
    byte[] identifyFrame = BuildFrame(0x14);

    foreach (string portName in SerialPort.GetPortNames()) {
        try {
            using var port = new SerialPort(portName, 115200) {
                ReadTimeout = 200,
                WriteTimeout = 200
            };
            port.Open();
            port.Write(identifyFrame, 0, identifyFrame.Length);

            byte[] buf = new byte[20];
            int total = 0;
            try {
                while (total < 16)
                    total += port.Read(buf, total, buf.Length - total);
            } catch (TimeoutException) {
                if (total < 16) continue;
            }

            // Find sync and validate
            int idx = Array.IndexOf(buf, (byte)0xAA);
            if (idx < 0 || idx + 16 > total) continue;
            if (buf[idx + 1] != 14) continue; // LEN

            // Check magic
            bool match = true;
            for (int i = 0; i < 4; i++)
                if (buf[idx + 3 + i] != magic[i]) { match = false; break; }
            if (!match) continue;

            // Extract hardware ID
            string hwId = Encoding.ASCII.GetString(buf, idx + 7, 8).TrimEnd('\0');
            return (portName, hwId);
        } catch (Exception) { continue; }
    }
    return null;
}
```

---

## Connection Lifecycle

```
Host                                  Deck
  │                                     │
  │──── CMD_IDENTIFY (0x14) ──────────→│  ← discovery (passive, no side effects)
  │                                     │
  │←─── IDENTIFY (0x07) ─────────────│  ← magic + hardware ID
  │                                     │
  │  (confirmed: this port is OpenDeck) │
  │                                     │
  │──── CMD_PING (0x10) ──────────────→│  ← activates serial mode
  │                                     │
  │←─── HEARTBEAT (0x04) ─────────────│  ← host is now "connected"
  │                                     │
  │──── CMD_INFO (0x11) ──────────────→│
  │                                     │
  │←─── DEVICE_INFO (0x05) ───────────│
  │                                     │
  │         ... normal operation ...    │
  │                                     │
  │←─── BTN_EVENT (0x01) ────────────│  ← user presses button
  │←─── BTN_EVENT (0x01) ────────────│  ← user releases button
  │←─── KNOB_ROTATE (0x02) ──────────│  ← user rotates encoder
  │                                     │
  │←─── HEARTBEAT (0x04) ─────────────│  ← every 3 seconds
  │                                     │
  │──── CMD_PING (0x10) ──────────────→│  ← keep-alive (recommended every 5s)
  │←─── HEARTBEAT (0x04) ─────────────│
  │                                     │
```

### Establishing Connection

1. **Discover** the device by sending **CMD_IDENTIFY** on each COM port (passive, safe)
2. Open the serial port at **115200 baud, 8N1**
3. Send **CMD_PING** (`AA 02 10 xx`) to **activate serial mode**
4. Wait for **HEARTBEAT** response (confirms deck is alive and serial mode is active)
5. Send **CMD_INFO** to discover capabilities
6. Start processing incoming events

### Keeping Connection Alive

The deck considers the host connected as long as it receives a message within the last **10 seconds**. Send CMD_PING every ~5 seconds to maintain the connection.

When the host is connected via serial, **BLE HID output is suppressed** — all events go exclusively to the serial host. When the host disconnects (timeout), BLE HID resumes automatically.

### Disconnecting

Simply stop sending pings. After 10 seconds of silence, the deck reverts to BLE HID mode. No explicit disconnect command is needed.

---

## Dual-Mode Behavior

| Serial Host | BLE Connected | Button Events Go To     |
|-------------|---------------|-------------------------|
| Yes         | Yes           | Serial only             |
| Yes         | No            | Serial only             |
| No          | Yes           | BLE HID                 |
| No          | No            | Nowhere (LED blinks)    |

Serial always takes priority. This allows the Windows app to fully control the device while connected, without interference from BLE HID keypresses.

---

## Timing Characteristics

| Parameter              | Value      |
|------------------------|------------|
| Baud rate              | 115200     |
| Button debounce        | 30ms       |
| Button scan interval   | 5ms        |
| Encoder scan interval  | 2ms        |
| Multi-click window     | 400ms      |
| Heartbeat interval     | 3s         |
| Host timeout           | 10s        |
| Typical event latency  | < 1ms (serial TX) + debounce |

**End-to-end latency** from physical button press to serial event: ~30-35ms (dominated by debounce time).

---

## Byte-Level Examples

### Auto-discovery (CMD_IDENTIFY)

**Host sends CMD_IDENTIFY:**
```
Bytes: AA 02 14 14
       │  │  │  └─ CRC8(0x14) = 0x14
       │  │  └──── type: CMD_IDENTIFY
       │  └─────── len: 2
       └────────── sync
```

**Deck responds with IDENTIFY (hardware_id = "ODK10K1"):**
```
Bytes: AA 0E 07 0D EC 4B 01 4F 44 4B 31 30 4B 31 00 xx
       │  │  │  └──────────┘ └──────────────────────┘ └─ CRC8
       │  │  │  magic bytes   "O  D  K  1  0  K  1" \0
       │  │  └──── type: IDENTIFY
       │  └─────── len: 14 (1 type + 12 payload + 1 crc)
       └────────── sync
```

### Ping/Heartbeat handshake

**Host sends CMD_PING:**
```
Bytes: AA 02 10 10
       │  │  │  └─ CRC8(0x10) = 0x10
       │  │  └──── type: CMD_PING
       │  └─────── len: 2
       └────────── sync
```

**Deck responds with HEARTBEAT (uptime=120s, BLE=connected):**
```
Bytes: AA 08 04 78 00 00 00 01 xx
       │  │  │  └──────────┘ │  └─ CRC8
       │  │  │  uptime=120   │
       │  │  │  (LE: 0x78)   └──── ble: connected
       │  │  └──── type: HEARTBEAT
       │  └─────── len: 8
       └────────── sync
```

### Button 5 press

```
Bytes: AA 04 01 05 01 xx
       │  │  │  │  │  └─ CRC8(01 05 01)
       │  │  │  │  └──── action: pressed
       │  │  │  └─────── index: 5
       │  │  └────────── type: BTN_EVENT
       │  └───────────── len: 4
       └──────────────── sync
```

### Encoder rotate CW

```
Bytes: AA 03 02 01 xx
       │  │  │  │  └─ CRC8(02 01)
       │  │  │  └──── direction: CW (up)
       │  │  └─────── type: KNOB_ROTATE
       │  └────────── len: 3
       └───────────── sync
```

### Request device info

**Host sends:**
```
Bytes: AA 02 11 11
```

**Deck responds (FW 2.0.0, 10 buttons, 1 knob, BLE capable):**
```
Bytes: AA 08 05 02 00 00 0A 01 01 xx
       │  │  │  │  │  │  │  │  │  └─ CRC8
       │  │  │  │  │  │  │  │  └──── flags: 0x01 (BLE)
       │  │  │  │  │  │  │  └─────── knobs: 1
       │  │  │  │  │  │  └────────── buttons: 10 (0x0A)
       │  │  │  │  │  └───────────── patch: 0
       │  │  │  │  └──────────────── minor: 0
       │  │  │  └─────────────────── major: 2
       │  │  └────────────────────── type: DEVICE_INFO
       │  └───────────────────────── len: 8
       └──────────────────────────── sync
```

---

## Parser State Machine (Host Side)

```
     ┌──────────┐
     │ RX_SYNC  │◄──────────────────────┐
     └────┬─────┘                       │
          │ byte == 0xAA                │
          ▼                             │
     ┌──────────┐                       │
     │ RX_LEN   │──── invalid len ─────┘
     └────┬─────┘                       │
          │ valid len (2-34)            │
          ▼                             │
     ┌──────────┐                       │
     │ RX_DATA  │──── all bytes read ──┘
     └────┬─────┘     → validate CRC
          │             → dispatch if OK
          │ more bytes needed
          └──► (loop)
```

### Python Example Parser

```python
import serial

SYNC = 0xAA

def parse_frames(port: serial.Serial):
    """Generator that yields (type, payload) tuples."""
    while True:
        # Wait for sync byte
        b = port.read(1)
        if not b or b[0] != SYNC:
            continue

        # Read length
        b = port.read(1)
        if not b:
            continue
        length = b[0]
        if length < 2 or length > 34:
            continue

        # Read TYPE + PAYLOAD + CRC
        data = port.read(length)
        if len(data) != length:
            continue

        # Validate CRC
        msg_type = data[0]
        payload = data[1:-1]
        received_crc = data[-1]
        computed_crc = crc8(bytes([msg_type]) + payload)

        if received_crc == computed_crc:
            yield (msg_type, payload)

def build_frame(msg_type: int, payload: bytes = b"") -> bytes:
    """Build a protocol frame ready to send."""
    crc_data = bytes([msg_type]) + payload
    crc = crc8(crc_data)
    length = 1 + len(payload) + 1  # type + payload + crc
    return bytes([SYNC, length, msg_type]) + payload + bytes([crc])
```

---

## Button Index Layout

```
┌───────┬───────┬───────┐
│ BTN 0 │ BTN 1 │ BTN 2 │  ← Otemu switches
├───────┼───────┼───────┤
│ BTN 3 │ BTN 4 │ BTN 5 │  ← Otemu switches
├───────┼───────┼───────┤
│ BTN 6 │ BTN 7 │ BTN 8 │  ← Otemu switches
└───────┴───────┴───────┘
        ┌─────┐
        │BTN 9│  ← encoder push button (also reports KNOB_CLICK)
        │ ENC │  ← rotation axis (reports KNOB_ROTATE)
        └─────┘

Total: 10 clickable buttons + 1 rotation axis
```

---

## Hardware Configuration Reference

| Component              | Count | GPIO Pins                           |
|------------------------|-------|-------------------------------------|
| Otemu switch buttons   | 9     | 4, 16, 17, 5, 18, 19, 21, 22, 23   |
| Encoder push button    | 1     | SW=25                               |
| Rotary encoder axis    | 1     | CLK=32, DT=33                       |
| Status LED             | 1     | GPIO 2 (onboard)                    |
| **Total buttons**      | **10**| BTN 0–8 (Otemu) + BTN 9 (encoder)  |
