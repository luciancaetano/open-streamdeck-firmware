#pragma once
// ============================================================================
// Comms Module — Centralized Communication Protocol Layer
// ============================================================================
//
// Owns the protocol format (currently JSON, designed for future binary).
// All message encoding, decoding, ACK generation, and transport routing
// live here. The transport modules (ble_serial, usb_serial) remain raw
// byte pipes — they never parse or build messages.
//
// To add a custom binary protocol in the future:
//   1. Add a COMMS_BINARY value to CommsProtocol
//   2. Implement encode/decode paths in comms.cpp
//   3. Transports stay unchanged — they just move bytes
//   4. A host command (e.g. {"protocol":"binary"}) can trigger the switch
//
// Architecture:
//   Input modules → comms_send_event()  → [encode] → transports → host
//   Host          → transports → [decode] → comms   → command callback
// ============================================================================

#include "config.h"

// Protocol format selector (extensible for future binary protocols)
enum CommsProtocol {
    COMMS_JSON,     // Current: newline-delimited JSON
    // COMMS_BINARY // Future: custom binary framing
};

// Command callback: receives a decoded command string, returns the command
// name if handled (e.g. "led", "brightness") or nullptr if unrecognized.
// Comms uses the return value to auto-generate ACK/NACK responses.
typedef const char* (*CommsCommandCallback)(const char* json);

// Initialize both transports (USB + BLE) and wire internal RX handlers.
void comms_init();

// Register the command handler that will receive decoded host commands.
void comms_set_command_callback(CommsCommandCallback cb);

// Poll both transports for incoming data. Call from main loop at
// SERIAL_RX_INTERVAL.
void comms_process_rx();

// Send an input event (button/knob/slider) to the host via all transports.
void comms_send_event(const char* json);

// Send a raw response string to the host (no protocol wrapping).
// Use for custom responses that don't fit the event/ACK pattern.
void comms_send_response(const char* json);

// Send the periodic heartbeat status message.
// Takes uptime in seconds and builds the protocol-appropriate message.
void comms_send_heartbeat(uint32_t uptimeSeconds);

// Returns true if a BLE client is currently connected.
bool comms_ble_connected();
