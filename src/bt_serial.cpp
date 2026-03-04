// ============================================================================
// Bluetooth Classic Serial Module - Implementation
// ============================================================================

#include "bt_serial.h"
#include <BluetoothSerial.h>

static BluetoothSerial SerialBT;
static BtRxCallback    rxCb = nullptr;
static String          rxBuffer;

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void bt_init() {
    SerialBT.begin(BT_DEVICE_NAME);
}

void bt_set_rx_callback(BtRxCallback cb) {
    rxCb = cb;
}

void bt_send(const char* json) {
    if (!SerialBT.hasClient()) return;
    SerialBT.println(json);
}

void bt_process_rx() {
    while (SerialBT.available()) {
        char c = SerialBT.read();
        if (c == '\n') {
            rxBuffer.trim();
            if (rxBuffer.length() > 0 && rxCb) {
                rxCb(rxBuffer.c_str());
            }
            rxBuffer = "";
        } else {
            rxBuffer += c;
        }
    }

    // Safety: if buffer grows too large without newlines, treat as one command
    if (rxBuffer.length() > SERIAL_RX_LINE_MAX) {
        if (rxCb) rxCb(rxBuffer.c_str());
        rxBuffer = "";
    }
}

bool bt_is_connected() {
    return SerialBT.hasClient();
}
