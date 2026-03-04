#pragma once
// ============================================================================
// USB Serial Module - Hardware UART communication
// ============================================================================
//
// Wraps the hardware Serial port for sending JSON events and receiving
// JSON commands from the host over USB.
// ============================================================================

#include "config.h"

// Callback type: called with a complete line received over USB Serial
typedef void (*UsbRxCallback)(const char* json);

// Initialize hardware serial at USB_SERIAL_BAUD.
void usb_init();

// Register a callback for incoming USB commands (complete lines).
void usb_set_rx_callback(UsbRxCallback cb);

// Send a JSON string over USB Serial (appends newline).
void usb_send(const char* json);

// Read available bytes and dispatch complete lines via callback.
// Should be called periodically from the main loop.
void usb_process_rx();
