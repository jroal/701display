#include "ble_telemetry.h"

#include <string.h>

void ble_telemetry_pack(const BleSnapshot &s, uint8_t out[BLE_PACKET_SIZE]) {
    auto put16 = [&](int i, uint16_t v) { out[i] = v & 0xFF; out[i + 1] = v >> 8; };
    float fuel = s.fuelLiters < 0 ? 0 : s.fuelLiters;
    float mpg = s.mpg < 0 ? 0 : s.mpg;
    uint32_t fuelCl = (uint32_t)(fuel * 100.0f + 0.5f);
    uint32_t mpgX10 = (uint32_t)(mpg * 10.0f + 0.5f);

    out[0] = BLE_PROTOCOL_VERSION;
    put16(1, s.rpm);
    put16(3, (uint16_t)s.coolantTempC);
    out[5] = s.gear;
    out[6] = (s.canActive ? 0x01 : 0x00) | ((s.absMode & 0x03) << 1) | ((s.sideStand & 0x03) << 3);
    put16(7, fuelCl > 0xFFFF ? 0xFFFF : fuelCl);
    out[9] = s.fuelPercent > 100 ? 100 : s.fuelPercent;
    put16(10, mpgX10 > 0xFFFF ? 0xFFFF : mpgX10);
}

#ifndef PC_SIMULATOR

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <stdio.h>

static BLEServer *bleServer = nullptr;
static BLECharacteristic *telemetryChar = nullptr;
static BleSnapshotFn snapshotFn = nullptr;
static BleCommandFn commandFn = nullptr;
static volatile bool bleConnected = false;
static char bleDeviceName[sizeof(BLE_DEVICE_NAME) + 7] = BLE_DEVICE_NAME;

const char *ble_telemetry_device_name() {
    return bleDeviceName;
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *) override { bleConnected = true; }
    void onDisconnect(BLEServer *server) override {
        bleConnected = false;
        server->getAdvertising()->start();
    }
};

class CommandCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *c) override {
        if (commandFn == nullptr) return;
        std::string value = c->getValue();
        if (!value.empty()) commandFn((uint8_t)value[0]);
    }
};

static void ble_task(void *) {
    uint8_t packet[BLE_PACKET_SIZE];
    while (true) {
        if (bleConnected && snapshotFn != nullptr) {
            BleSnapshot snap;
            snapshotFn(snap);
            ble_telemetry_pack(snap, packet);
            telemetryChar->setValue(packet, sizeof(packet));
            telemetryChar->notify();
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void ble_telemetry_begin(BleSnapshotFn snapshot, BleCommandFn onCommand) {
    snapshotFn = snapshot;
    commandFn = onCommand;

    snprintf(bleDeviceName, sizeof(bleDeviceName), "%s-%06lX", BLE_DEVICE_NAME,
             (unsigned long)(ESP.getEfuseMac() & 0xFFFFFF));
    BLEDevice::init(bleDeviceName);
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new ServerCallbacks());

    BLEService *service = bleServer->createService(BLE_SERVICE_UUID);

    telemetryChar = service->createCharacteristic(
        BLE_TELEMETRY_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    telemetryChar->addDescriptor(new BLE2902());

    BLECharacteristic *commandChar = service->createCharacteristic(
        BLE_COMMAND_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    commandChar->setCallbacks(new CommandCallbacks());

    service->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SERVICE_UUID);
    BLEAdvertisementData scanResponseData;
    scanResponseData.setName(bleDeviceName);
    adv->setScanResponseData(scanResponseData);
    adv->setScanResponse(true);
    adv->start();

    xTaskCreatePinnedToCore(ble_task, "BLETask", 4096, NULL, 0, NULL, 0);
}

#endif
