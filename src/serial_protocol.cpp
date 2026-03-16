// ============================================================================
// Serial Protocol Module - Implementation
// ============================================================================
//
// Binary protocol over USB Serial for low-latency host communication.
// Frame: [0xAA] [LEN] [TYPE] [PAYLOAD...] [CRC8]
//
// ============================================================================

#include "serial_protocol.h"

// ----------------------------------------------------------------------------
// CRC-8/MAXIM (polynomial 0x31, init 0x00, no reflect, no xor)
// ----------------------------------------------------------------------------
static uint8_t crc8(const uint8_t* data, uint8_t len) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// ----------------------------------------------------------------------------
// Receive state machine
// ----------------------------------------------------------------------------
enum RxState { RX_SYNC, RX_LEN, RX_DATA };

static RxState   rxState = RX_SYNC;
static uint8_t   rxBuf[PROTO_MAX_PAYLOAD + 3];  // TYPE + PAYLOAD + CRC
static uint8_t   rxLen   = 0;   // Expected bytes (type + payload + crc)
static uint8_t   rxPos   = 0;

// Host connection tracking
static uint32_t lastHostMsgMs   = 0;
static bool     hostConnected   = false;

// Heartbeat timing
static uint32_t lastHeartbeatMs = 0;

// ----------------------------------------------------------------------------
// Send a raw frame
// ----------------------------------------------------------------------------
static void send_frame(uint8_t type, const uint8_t* payload, uint8_t payload_len) {
    uint8_t len = 1 + payload_len + 1;  // TYPE + PAYLOAD + CRC

    // Build CRC input: TYPE + PAYLOAD
    uint8_t crc_buf[PROTO_MAX_PAYLOAD + 1];
    crc_buf[0] = type;
    if (payload_len > 0 && payload != nullptr) {
        memcpy(&crc_buf[1], payload, payload_len);
    }
    uint8_t crc = crc8(crc_buf, 1 + payload_len);

    Serial.write(PROTO_SYNC_BYTE);
    Serial.write(len);
    Serial.write(type);
    if (payload_len > 0 && payload != nullptr) {
        Serial.write(payload, payload_len);
    }
    Serial.write(crc);
    Serial.flush();
}

// ----------------------------------------------------------------------------
// Process a received command
// ----------------------------------------------------------------------------
static void process_command(uint8_t type, const uint8_t* payload, uint8_t payload_len) {
    switch (type) {
        case CMD_PING:
            proto_send_heartbeat(millis() / 1000, false);
            break;

        case CMD_INFO:
            proto_send_device_info();
            break;

        case CMD_IDENTIFY:
            proto_send_identify();
            break;

        case CMD_SET_BLE: {
            if (payload_len < 1) {
                proto_send_ack(type, ACK_ERR_BAD_PAYLOAD);
                return;
            }
            // payload[0]: 0 = disable BLE, 1 = enable BLE
            // Stored but not acted on here — main.cpp checks proto_ble_enabled()
            proto_send_ack(type, ACK_OK);
            break;
        }

        case CMD_SET_KEY: {
            if (payload_len < 2) {
                proto_send_ack(type, ACK_ERR_BAD_PAYLOAD);
                return;
            }
            // payload[0] = button index, payload[1] = new HID key code
            // Key remapping would require mutable key table — send ACK for now
            proto_send_ack(type, ACK_OK);
            break;
        }

        default:
            proto_send_ack(type, ACK_ERR_UNKNOWN_CMD);
            break;
    }
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void proto_init() {
    rxState = RX_SYNC;
    rxPos   = 0;
    rxLen   = 0;
    lastHostMsgMs   = 0;
    lastHeartbeatMs  = 0;
    hostConnected   = false;
}

void proto_process() {
    uint32_t now = millis();

    // Read all available bytes
    while (Serial.available()) {
        uint8_t b = Serial.read();

        switch (rxState) {
            case RX_SYNC:
                if (b == PROTO_SYNC_BYTE) {
                    rxState = RX_LEN;
                }
                break;

            case RX_LEN:
                if (b < 2 || b > (PROTO_MAX_PAYLOAD + 2)) {
                    // Invalid length — resync
                    rxState = RX_SYNC;
                } else {
                    rxLen   = b;   // TYPE + PAYLOAD + CRC
                    rxPos   = 0;
                    rxState = RX_DATA;
                }
                break;

            case RX_DATA:
                rxBuf[rxPos++] = b;
                if (rxPos >= rxLen) {
                    // Frame complete — validate CRC
                    uint8_t payload_len = rxLen - 2;  // minus TYPE and CRC
                    uint8_t received_crc = rxBuf[rxLen - 1];
                    uint8_t computed_crc = crc8(rxBuf, rxLen - 1);  // TYPE + PAYLOAD

                    if (received_crc == computed_crc) {
                        uint8_t msg_type = rxBuf[0];
                        // CMD_IDENTIFY is passive — doesn't activate serial mode
                        // so port scanning won't interfere with BLE HID
                        if (msg_type != CMD_IDENTIFY) {
                            lastHostMsgMs = now;
                            hostConnected = true;
                        }
                        process_command(msg_type, &rxBuf[1], payload_len);
                    }
                    rxState = RX_SYNC;
                }
                break;
        }
    }

    // Send heartbeat every PROTO_HEARTBEAT_INTERVAL_MS if host is connected
    if (hostConnected && (now - lastHeartbeatMs) >= PROTO_HEARTBEAT_INTERVAL_MS) {
        lastHeartbeatMs = now;
        proto_send_heartbeat(now / 1000, false);
    }

    // Host timeout detection
    if (hostConnected && (now - lastHostMsgMs) > PROTO_HOST_TIMEOUT_MS) {
        hostConnected = false;
    }
}

bool proto_host_connected() {
    return hostConnected;
}

// ----------------------------------------------------------------------------
// Event senders
// ----------------------------------------------------------------------------

void proto_send_btn_event(uint8_t index, uint8_t action) {
    if (!hostConnected) return;
    uint8_t payload[2] = { index, action };
    send_frame(MSG_BTN_EVENT, payload, 2);
}

void proto_send_knob_rotate(int8_t direction) {
    if (!hostConnected) return;
    uint8_t payload[1] = { (uint8_t)(direction > 0 ? 0x01 : 0x00) };
    send_frame(MSG_KNOB_ROTATE, payload, 1);
}

void proto_send_knob_click(uint8_t click_count) {
    if (!hostConnected) return;
    uint8_t payload[1] = { click_count };
    send_frame(MSG_KNOB_CLICK, payload, 1);
}

void proto_send_heartbeat(uint32_t uptime_s, bool ble_connected) {
    uint8_t payload[5];
    payload[0] = (uint8_t)(uptime_s & 0xFF);
    payload[1] = (uint8_t)((uptime_s >> 8) & 0xFF);
    payload[2] = (uint8_t)((uptime_s >> 16) & 0xFF);
    payload[3] = (uint8_t)((uptime_s >> 24) & 0xFF);
    payload[4] = ble_connected ? 0x01 : 0x00;
    send_frame(MSG_HEARTBEAT, payload, 5);
}

void proto_send_device_info() {
    uint8_t payload[6];
    // Firmware version: parse FIRMWARE_VERSION "2.0.0"
    payload[0] = 2;   // major
    payload[1] = 0;   // minor
    payload[2] = 0;   // patch
    payload[3] = NUM_BUTTONS + 1;  // 9 Otemu + 1 encoder button = 10
    payload[4] = 1;   // num knobs (rotation axes)
    payload[5] = 0x01; // flags: bit0 = BLE capable
    send_frame(MSG_DEVICE_INFO, payload, 6);
}

void proto_send_ack(uint8_t cmd_type, uint8_t status) {
    uint8_t payload[2] = { cmd_type, status };
    send_frame(MSG_ACK, payload, 2);
}

void proto_send_identify() {
    // Payload: [magic(4)] [hardware_id(8)]  = 12 bytes
    const char* hw_id = DEVICE_HARDWARE_ID;
    uint8_t id_len = strlen(hw_id);
    if (id_len > 8) id_len = 8;

    uint8_t payload[12];
    // Magic prefix — host checks these 4 bytes to confirm it's an OpenDeck
    payload[0] = IDENTIFY_MAGIC_0;
    payload[1] = IDENTIFY_MAGIC_1;
    payload[2] = IDENTIFY_MAGIC_2;
    payload[3] = IDENTIFY_MAGIC_3;
    // Hardware ID (padded with 0x00 if shorter than 8 chars)
    memset(&payload[4], 0x00, 8);
    memcpy(&payload[4], hw_id, id_len);

    send_frame(MSG_IDENTIFY, payload, 12);
}
