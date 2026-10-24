# BLE Protocol (ESP32 display ⇄ Mobile_701 app)

The ESP32 runs a BLE GATT server with an advertised device name in the form `Husky701-XXXXXX`, where the six-hex-digit suffix is derived from the chip's eFuse MAC address. It advertises the service UUID. The Android app scans for the service, lists nearby displays by name and address, connects to the selected display, subscribes to telemetry notifications (~5 Hz), and can send commands.

| Item | UUID | Properties |
|------|------|------------|
| Service | `7a010000-701d-4a5e-9f3b-0701d15b1e00` | |
| Telemetry | `7a010001-701d-4a5e-9f3b-0701d15b1e00` | read, notify |
| Command | `7a010002-701d-4a5e-9f3b-0701d15b1e00` | write, write without response |

## Telemetry packet (12 bytes, little-endian)

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8 | protocol version (1) |
| 1 | u16 | engine RPM |
| 3 | i16 | coolant temperature, °C |
| 5 | u8 | gear (0 = neutral) |
| 6 | u8 | flags: bit0 CAN active, bits1-2 ABS mode (0 on, 1 off-road, 2 off), bits3-4 side stand (0 unknown, 1 down, 2 up) |
| 7 | u16 | fuel remaining, 0.01 L |
| 9 | u8 | fuel percent (0-100) |
| 10 | u16 | economy, 0.1 MPG |

The app derives gallons, °F and range (`gallons × MPG`) with the same formulas as the display. The clock shown in the app is the phone's clock.

## Commands (1 byte)

| Value | Action |
|-------|--------|
| `0x01` | Reset fuel (same as the on-screen Reset button) |

Code: `src/ble_telemetry.{h,cpp}` (firmware), `APK/mobile_701/lib/{telemetry,ble_client,dashboard}.dart` (app).
