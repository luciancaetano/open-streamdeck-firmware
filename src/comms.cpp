// ============================================================================
// Comms Module — Implementation
// ============================================================================
//
// Currently implements the JSON protocol. All message building and parsing
// is concentrated here so that adding a binary protocol later requires
// changes only in this file.
// ============================================================================

#include "comms.h"
#include "ble_serial.h"
#include "usb_serial.h"
#include <ArduinoJson.h>

static CommsCommandCallback cmdCb = nullptr;

// ----------------------------------------------------------------------------
// Internal: send a pre-formatted string to all active transports
// ----------------------------------------------------------------------------
static void broadcast(const char* data) {
    usb_send(data);
    ble_send(data);
}

// ----------------------------------------------------------------------------
// Internal: build and send an ACK/NACK response
// ----------------------------------------------------------------------------
static void sendAck(bool success, const char* detail) {
    StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
    doc["ack"] = success;
    if (success) {
        doc["cmd"] = detail;
    } else {
        doc["error"] = detail;
    }
    char buf[JSON_TX_BUFFER_SIZE];
    serializeJson(doc, buf);
    broadcast(buf);
}

// ----------------------------------------------------------------------------
// Internal: handle a raw line received from any transport
// ----------------------------------------------------------------------------
static void onRawRx(const char* line) {
    // --- JSON protocol decode ---
    StaticJsonDocument<JSON_RX_BUFFER_SIZE> doc;
    DeserializationError err = deserializeJson(doc, line);
    if (err) {
        sendAck(false, "parse_error");
        return;
    }

    // Dispatch to the registered command handler
    if (cmdCb) {
        const char* cmdName = cmdCb(line);
        if (cmdName) {
            sendAck(true, cmdName);
        } else {
            sendAck(false, "unknown_command");
        }
    }
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void comms_init() {
    usb_init();
    ble_init();

    // Wire both transports to our internal decoder
    usb_set_rx_callback(onRawRx);
    ble_set_rx_callback(onRawRx);
}

void comms_set_command_callback(CommsCommandCallback cb) {
    cmdCb = cb;
}

void comms_process_rx() {
    usb_process_rx();
    ble_process_rx();
}

void comms_send_event(const char* json) {
    // --- JSON protocol: send as-is (already serialized by input modules) ---
    // Future binary: re-encode from JSON doc to binary frame here
    broadcast(json);
}

void comms_send_response(const char* json) {
    broadcast(json);
}

void comms_send_heartbeat(uint32_t uptimeSeconds) {
    // --- JSON protocol encode ---
    StaticJsonDocument<JSON_TX_BUFFER_SIZE> doc;
    doc["status"] = "alive";
    doc["uptime"] = uptimeSeconds;
    doc["ble"]    = ble_is_connected();
    char buf[JSON_TX_BUFFER_SIZE];
    serializeJson(doc, buf);
    broadcast(buf);
}

bool comms_ble_connected() {
    return ble_is_connected();
}
