#pragma once
// ============================================================================
// BLE Serial Module - NimBLE-based Nordic UART service
// ============================================================================
//
// Provides BLE connectivity using the Nordic UART Service (NUS) profile.
// Supports sending JSON events to the host and receiving JSON commands.
// ============================================================================

#include "config.h"

// Callback type: called with a complete line received over BLE
typedef void (*BleRxCallback)(const char* json);

// Initialize NimBLE stack, create service/characteristics, start advertising.
void ble_init();

// Register a callback for incoming BLE commands (complete lines).
void ble_set_rx_callback(BleRxCallback cb);

// Send a JSON string to the connected BLE host (via NOTIFY).
// No-op if no client is connected.
void ble_send(const char* json);

// Process any buffered incoming BLE data into complete lines.
// Should be called periodically from the main loop.
void ble_process_rx();

// Returns true if a BLE client is currently connected.
bool ble_is_connected();
