#pragma once
#include <stdint.h>

// BLE GATT interface between the ESP32 display and the Mobile_701 Android app.
// Full protocol description: Docs/BLE_Protocol.md
//
// Service  : 7a010000-701d-4a5e-9f3b-0701d15b1e00
//  TELEMETRY (read|notify) : 12-byte little-endian packet, see BleSnapshot / ble_telemetry_pack()
//  COMMAND   (write)       : 1 byte, 0x01 = reset fuel (same as the on-screen Reset button)

#define BLE_DEVICE_NAME          "Husky701"
#define BLE_SERVICE_UUID         "7a010000-701d-4a5e-9f3b-0701d15b1e00"
#define BLE_TELEMETRY_CHAR_UUID  "7a010001-701d-4a5e-9f3b-0701d15b1e00"
#define BLE_COMMAND_CHAR_UUID    "7a010002-701d-4a5e-9f3b-0701d15b1e00"

#define BLE_PROTOCOL_VERSION     1
#define BLE_PACKET_SIZE          12
#define BLE_CMD_RESET_FUEL       0x01

// Values exactly as the display UI consumes them (metric units, converted by the app).
struct BleSnapshot {
    uint16_t rpm;
    int16_t  coolantTempC;
    uint8_t  gear;        // 0 = neutral
    bool     canActive;
    uint8_t  absMode;     // 0 = on, 1 = off-road, 2 = off
    uint8_t  sideStand;   // 0 = unknown, 1 = down, 2 = up
    float    fuelLiters;
    uint8_t  fuelPercent; // 0..100
    float    mpg;
};

typedef void (*BleSnapshotFn)(BleSnapshot &out);
typedef void (*BleCommandFn)(uint8_t command);

void ble_telemetry_pack(const BleSnapshot &s, uint8_t out[BLE_PACKET_SIZE]);

#ifndef PC_SIMULATOR
// Starts the BLE server and a task that notifies subscribers ~5 times per second.
void ble_telemetry_begin(BleSnapshotFn snapshot, BleCommandFn onCommand);
#endif
