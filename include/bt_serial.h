#pragma once
// ============================================================================
// Bluetooth Classic Serial Module - ESP32 SPP (Serial Port Profile)
// ============================================================================
//
// Provides Bluetooth Classic connectivity using SPP.
// The device appears as a standard Bluetooth device in Windows and creates
// a virtual COM port for serial communication.
// ============================================================================

#include "config.h"

// Callback type: called with a complete line received over Bluetooth
typedef void (*BtRxCallback)(const char* json);

// Initialize Bluetooth Classic SPP with the configured device name.
void bt_init();

// Register a callback for incoming Bluetooth commands (complete lines).
void bt_set_rx_callback(BtRxCallback cb);

// Send a JSON string to the connected Bluetooth host.
// No-op if no client is connected.
void bt_send(const char* json);

// Process any buffered incoming Bluetooth data into complete lines.
// Should be called periodically from the main loop.
void bt_process_rx();

// Returns true if a Bluetooth client is currently connected.
bool bt_is_connected();
