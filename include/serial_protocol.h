#pragma once
// ============================================================================
// Serial Protocol Module - Low-latency binary protocol for host communication
// ============================================================================
//
// Frame format: [0xAA] [LEN] [TYPE] [PAYLOAD...] [CRC8]
//   0xAA  = sync byte
//   LEN   = number of bytes after LEN (type + payload + crc)
//   TYPE  = message type ID
//   CRC8  = CRC-8/MAXIM over TYPE + PAYLOAD
//
// ============================================================================

#include "config.h"

// Frame constants
#define PROTO_SYNC_BYTE       0xAA
#define PROTO_MAX_PAYLOAD     32
#define PROTO_HEADER_SIZE     3    // SYNC + LEN + TYPE
#define PROTO_OVERHEAD        4    // SYNC + LEN + TYPE + CRC

// ----------------------------------------------------------------------------
// Message types: Deck → Host (0x01 - 0x0F)
// ----------------------------------------------------------------------------
#define MSG_BTN_EVENT         0x01  // Button press/release
#define MSG_KNOB_ROTATE       0x02  // Encoder rotation
#define MSG_KNOB_CLICK        0x03  // Encoder button click(s)
#define MSG_HEARTBEAT         0x04  // Periodic alive signal
#define MSG_DEVICE_INFO       0x05  // Device info response
#define MSG_ACK               0x06  // Command acknowledgement
#define MSG_IDENTIFY          0x07  // Hardware identity response

// ----------------------------------------------------------------------------
// Message types: Host → Deck (0x10 - 0x1F)
// ----------------------------------------------------------------------------
#define CMD_PING              0x10  // Ping (expects heartbeat back)
#define CMD_INFO              0x11  // Request device info
#define CMD_SET_BLE           0x12  // Enable/disable BLE HID
#define CMD_SET_KEY           0x13  // Remap a button's HID key
#define CMD_IDENTIFY          0x14  // Request hardware ID (for auto-discovery)

// ----------------------------------------------------------------------------
// Button actions
// ----------------------------------------------------------------------------
#define ACTION_RELEASED       0x00
#define ACTION_PRESSED        0x01

// ACK status
#define ACK_OK                0x00
#define ACK_ERR_UNKNOWN_CMD   0x01
#define ACK_ERR_BAD_PAYLOAD   0x02

// Identify magic bytes (fixed prefix in MSG_IDENTIFY response)
#define IDENTIFY_MAGIC_0      0x0D  // 'OD' compressed
#define IDENTIFY_MAGIC_1      0xEC
#define IDENTIFY_MAGIC_2      0x4B  // 'K'
#define IDENTIFY_MAGIC_3      0x01  // protocol version

// ----------------------------------------------------------------------------
// API
// ----------------------------------------------------------------------------

void proto_init();
void proto_process();   // Call from loop() to process incoming commands

// Send events to host
void proto_send_btn_event(uint8_t index, uint8_t action);
void proto_send_knob_rotate(int8_t direction);
void proto_send_knob_click(uint8_t click_count);
void proto_send_heartbeat(uint32_t uptime_s, bool ble_connected);
void proto_send_device_info();
void proto_send_ack(uint8_t cmd_type, uint8_t status);
void proto_send_identify();

// State
bool proto_host_connected();  // True if host sent a message recently
