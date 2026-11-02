# BLE Protocol (ESP32 display ⇄ Mobile_701 app)

The ESP32 runs a BLE GATT server with an advertised device name in the form `Husky701-XXXXXX`, where the six-hex-digit suffix is derived from the chip's eFuse MAC address. It advertises the service UUID. The Android app scans for the service, lists nearby displays by name and address, connects to the selected display, subscribes to telemetry notifications (~5 Hz), and can send commands.

| Item | UUID | Properties |
|------|------|------------|
| Service | `7a010000-701d-4a5e-9f3b-0701d15b1e00` | |
| Telemetry | `7a010001-701d-4a5e-9f3b-0701d15b1e00` | read, notify |
| Command | `7a010002-701d-4a5e-9f3b-0701d15b1e00` | write, write without response |
| Info | `7a010003-701d-4a5e-9f3b-0701d15b1e00` | read (protocol 2+) |

## Versioning

The protocol version is `BLE_PROTOCOL_VERSION` in `src/ble_telemetry.h` (telemetry byte 0) and `Telemetry.protocolVersion` in the app. **Increment it whenever the GATT layout, telemetry packet or command set changes**, add a row below, and update the app's supported range. The app accepts `Telemetry.minProtocolVersion`..`protocolVersion`.

The firmware version (`FIRMWARE_VERSION`, same header; shared by the `43b` and `headless` builds) and the app version (`pubspec.yaml`) are independent and must also be bumped on release.

| Protocol | Change |
|----------|--------|
| 1 | Initial 12-byte telemetry; commands 0x01-0x03 |
| 2 | Adds Info characteristic; telemetry layout unchanged |

The Info value is ASCII `fw=<firmware>;proto=<protocol>;build=<43b|headless>`. The app shows it, with its own version, under Settings → About. The 43b display shows firmware and protocol versions when the CAN label is tapped; the headless build relies on the app.

## Telemetry packet (12 bytes, little-endian)

| Offset | Type | Field |
|--------|------|-------|
| 0 | u8 | protocol version (see Versioning) |
| 1 | u16 | engine RPM |
| 3 | i16 | coolant temperature, °C |
| 5 | u8 | gear (0 = neutral) |
| 6 | u8 | flags: bit0 CAN active, bits1-2 ABS mode (0 on, 1 off-road, 2 off), bits3-4 side stand (0 unknown, 1 down, 2 up), bit5 off-road ABS requested (setting stored on the ESP32) |
| 7 | u16 | fuel remaining, 0.01 L |
| 9 | u8 | fuel percent (0-100) |
| 10 | u16 | economy, 0.1 MPG |

The app derives gallons, °F and range (`gallons × MPG`) with the same formulas as the display. The app has no clock.

## Commands (1 byte)

| Value | Action |
|-------|--------|
| `0x01` | Reset fuel (same as the on-screen Reset button) |
| `0x02` | Request off-road ABS (saved in ESP32 NVM; takes effect on the next key cycle) |
| `0x03` | Clear the off-road ABS request |

Code: `src/ble_telemetry.{h,cpp}` (firmware), `APK/mobile_701/lib/{telemetry,ble_client,dashboard}.dart` (app).

## Version compatibility

| BLE protocol | ESP32 firmware | Compatible Android app |
|--------------|----------------|------------------------|
| 1 | Earlier firmware (firmware version not recorded) | `1.0.4+4`; `1.1.0+5` |
| 2 | `1.0.0` (`43b` or `headless`) | `1.1.0+5` |

App `1.0.4+4` accepts protocol 1 only. App `1.1.0+5` accepts protocols 1 through 2, so it works with both firmware generations.
