// ============================================================================
// BLE Serial Module - Implementation
// ============================================================================

#include "ble_serial.h"
#include <NimBLEDevice.h>

// Internal state
static NimBLEServer*         server  = nullptr;
static NimBLECharacteristic* txChar  = nullptr;
static NimBLECharacteristic* rxChar  = nullptr;
static bool                  connected = false;
static String                rxBuffer;
static BleRxCallback         rxCb   = nullptr;

// ----------------------------------------------------------------------------
// BLE Callbacks
// ----------------------------------------------------------------------------

class ServerCB : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer*) override   { connected = true; }
    void onDisconnect(NimBLEServer*) override {
        connected = false;
        NimBLEDevice::startAdvertising();
    }
};

class RxCB : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        std::string val = c->getValue();
        rxBuffer += val.c_str();
    }
};

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void ble_init() {
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);

    server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCB());

    NimBLEService* svc = server->createService(BLE_SERVICE_UUID);

    txChar = svc->createCharacteristic(
        BLE_TX_CHAR_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );

    rxChar = svc->createCharacteristic(
        BLE_RX_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    rxChar->setCallbacks(new RxCB());

    svc->start();

    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinInterval(BLE_ADV_INTERVAL_MIN);
    adv->setMaxInterval(BLE_ADV_INTERVAL_MAX);
    adv->start();
}

void ble_set_rx_callback(BleRxCallback cb) {
    rxCb = cb;
}

void ble_send(const char* json) {
    if (!connected || !txChar) return;

    txChar->setValue((const uint8_t*)json, strlen(json));
    txChar->notify();
}

void ble_process_rx() {
    if (rxBuffer.length() == 0) return;

    // Extract and dispatch complete newline-terminated lines
    int nlPos;
    while ((nlPos = rxBuffer.indexOf('\n')) >= 0) {
        String line = rxBuffer.substring(0, nlPos);
        line.trim();
        if (line.length() > 0 && rxCb) {
            rxCb(line.c_str());
        }
        rxBuffer = rxBuffer.substring(nlPos + 1);
    }

    // Safety: if buffer grows too large without newlines, treat it as one command
    if (rxBuffer.length() > SERIAL_RX_LINE_MAX) {
        if (rxCb) rxCb(rxBuffer.c_str());
        rxBuffer = "";
    }
}

bool ble_is_connected() {
    return connected;
}
