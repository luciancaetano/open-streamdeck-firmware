// ============================================================================
// USB Serial Module - Implementation
// ============================================================================

#include "usb_serial.h"

static char          rxBuf[SERIAL_RX_LINE_MAX];
static size_t        rxLen = 0;
static UsbRxCallback rxCb  = nullptr;

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void usb_init() {
    Serial.begin(USB_SERIAL_BAUD);
}

void usb_set_rx_callback(UsbRxCallback cb) {
    rxCb = cb;
}

void usb_send(const char* json) {
    Serial.println(json);
}

void usb_process_rx() {
    while (Serial.available()) {
        char c = Serial.read();

        if (c == '\n' || c == '\r') {
            if (rxLen > 0) {
                rxBuf[rxLen] = '\0';
                if (rxCb) rxCb(rxBuf);
                rxLen = 0;
            }
        } else if (rxLen < SERIAL_RX_LINE_MAX - 1) {
            rxBuf[rxLen++] = c;
        }
    }
}
