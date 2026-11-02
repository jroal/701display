# Husky 701 Display – Owner's Manual

Secondary display and virtual fuel computer for the Husqvarna 701 Enduro (Euro 5). It reads the motorcycle's CAN bus through the OBD port and shows fuel, range, economy, gear, RPM, coolant temperature, ABS mode and side-stand state.

> **Safety:** This is an experimental secondary display. It does not replace the factory dash or fuel warning light. Never operate or adjust it while riding. See the disclaimer in [README.md](README.md).

## Contents

1. [System overview](#1-system-overview)
2. [What you need](#2-what-you-need)
3. [Wiring](#3-wiring)
4. [Installing the firmware](#4-installing-the-firmware)
5. [Installing the Android app](#5-installing-the-android-app)
6. [First-time setup](#6-first-time-setup)
7. [Display screen reference](#7-display-screen-reference)
8. [Functions and settings](#8-functions-and-settings)
9. [Android app settings](#9-android-app-settings)
10. [Versions and compatibility](#10-versions-and-compatibility)
11. [SD card logging (display build)](#11-sd-card-logging-display-build)
12. [Troubleshooting](#12-troubleshooting)

---

## 1. System overview

There are two hardware options. Both run the same CAN/fuel logic and both broadcast live data over Bluetooth Low Energy (BLE) at about 20 Hz.

| | **Display build** | **Headless build** |
|---|---|---|
| Hardware | Waveshare ESP32-S3-Touch-LCD-4.3B (800×480 touchscreen) | Waveshare ESP32-S3-RS485-CAN (no screen) |
| PlatformIO env | `waveshare_esp32s3_43b` | `waveshare_esp32s3_rs485_can` |
| Shows data on | Its own touchscreen **and** the Android app | The Android app only |
| Touch settings | Yes (fuel reset, off-road ABS, clock) | No (use the app) |
| Real-time clock, SD logging | Yes | No |

The Android app (a phone or motorcycle tablet) shows the same dashboard as the 4.3" screen and can change the same settings.

## 2. What you need

- Motorcycle: Husqvarna 701 Enduro, Euro 5 (developed on a 2023 US model). The ABS ECU must be working, because the display needs data from both the engine and ABS ECUs.
- One of the ESP32 boards above.
- Power (12 V switched), ground, and the two CAN wires from the bike's OBD/diagnostic port. A T-harness lets other devices (scan tool, cruise control) stay connected.
- Display build: a coin cell battery for the real-time clock. It is required for the ESP32 clock and for data logs. Optionally, a microSD card for data and error logs.
- Android phone/tablet with Bluetooth LE and Google Play (or the ability to sideload).
- For building the firmware yourself: Visual Studio Code with PlatformIO. For a prebuilt install, a Chromium-based browser and a USB cable.

## 3. Wiring

Connect the board to the OBD port:

1. **Power**: switched 12 V (key-on) and ground, through the board's supply input.
2. **CAN High / CAN Low**: to the CAN pins of the OBD connector.

The CAN bus runs at **500 kbps**. Do not power the board from an always-on feed unless you accept the board staying on when the key is off.

The author's harness uses 5-pin e-bike connectors at the display end so the display can be removed without unplugging the bike harness. The OBD connector pinout is shown below.

<img width="986" height="736" alt="HuskyOBDFace" src="https://github.com/user-attachments/assets/7b9ec2c9-35ec-4e07-a771-2fb836d68805" />

> Connecting third-party devices to the diagnostic bus carries risk. Double-check polarity and pin assignments before powering up.

## 4. Installing the firmware

### Option A – Web installer (no tools)

1. Connect the ESP32 to your computer by USB.
2. Open <https://jroal.github.io/701display/installer.html> in Chrome or Edge.
3. Follow the prompts to select the serial port and flash.

### Option B – PlatformIO

1. Install VS Code and the PlatformIO extension, then open the project folder.
2. Select the environment for your board and upload:

   | Board | Command |
   |---|---|
   | Display (4.3B) | `pio run -e waveshare_esp32s3_43b -t upload` |
   | Headless | `pio run -e waveshare_esp32s3_rs485_can -t upload` |

   In VS Code you can also use **Terminal → Run Task → Build/Upload Headless Hardware**.
3. Do **not** use the `waveshare_esp32s3_43b_capture` environment for normal use. It is a diagnostic build that records every CAN frame.

## 5. Installing the Android app

- **Google Play:** install it from the [Play Store listing](https://play.google.com/store/apps/details?id=com.jroal.mobile_701_display).
- **Sideload:** build with `flutter build apk --release` from `APK\mobile_701` and install the resulting `app-release.apk`.

On first launch, allow the Bluetooth ("Nearby devices") permission when asked. If you enable the fuel-range overlay, you will also be asked for the "display over other apps" permission.

## 6. First-time setup

1. **Wire and flash** the ESP32 (sections 3 and 4).
2. **Key the bike ON.** The ESP32 powers up and starts reading the CAN bus. The `CAN` indicator turns green when valid bike data is arriving.
3. **Set the clock** (display build): tap the date/time and set it (section 8.3). This is used for log file names and timestamps.
4. **Fill the tank, then press Reset** (section 8.1). The fuel gauge is an estimate, so it must be calibrated at every fill-up.
5. **Pair the app:** open the Android app. It scans for a display named `Husky701-XXXXXX`. If more than one is nearby, open **Settings** (gear icon) and pick yours under "701 display to connect to". You should always select your specific ESP32 to connect to otherwise you risk connecting to another nearby ESP32. Start by letting it connect to any while your bike is the only one with the ESP32 on it in the area (within 200 feet or so). Then find which one it connected to and select that in the list. Once selected it will always look for only that ESP32. 
6. Confirm the app's top-left status reads `BLE: connected`.

## 7. Display screen reference

The 4.3" display and the Android app show the same layout.

| Element | Location | Meaning |
|---|---|---|
| **Fuel bar** and `E 1/4 1/2 3/4 F` scale | Top left | Estimated fuel remaining as a percent of the 13 L usable capacity. The percent is shown inside the bar. |
| **FUEL: x.xx gal** | Top right | Estimated fuel remaining in US gallons. |
| **Reset** button | Top centre | Tap after filling the tank (section 8.1). |
| Date / time | Top right, under FUEL | Real-time clock (display build). Tap to set. |
| **TEMP: xx F** | Left, middle | Engine coolant temperature in °F. |
| **GEAR: n** | Right, middle | Current gear (`N` = neutral). |
| **RPM bar** and `xxxx RPM` | Centre | Engine speed. Bar colour: green below 8000, yellow 8000–9000, red above 9000. Scale ends at 10,000. |
| **ECON: x.x** | Lower left | Fuel economy in MPG. |
| **RANGE: xx mi** | Lower right | Estimated remaining range (fuel left × economy). Shows `--` until economy is known. |
| **STAND: UP / DOWN / --** | Bottom left (upper line) | Side stand. `DOWN` is red, `UP` is green, `--` means unknown. |
| **ABS: ON / OFF-Road** | Bottom left | ABS mode reported by the bike. See the note below. |
| **Off-road ABS** switch | Bottom centre | Requests off-road ABS mode (section 8.2). |
| **CAN** | Bottom right | Green = receiving bike CAN data. Yellow = no data. On the display build, tap it to see versions. |
| BLE status (app only) | Top left of the app | Search/connection state of the Bluetooth link. |

> **ABS note:** The display shows only **ABS: ON** or **ABS: OFF-Road**. When the rider switches ABS completely off with the button, the bike does not broadcast that state in a way the display can read, so it will still show ON (or OFF-Road). Rely on the bike's own ABS indicator.

## 8. Functions and settings

### 8.1 Fuel gauge, economy and range

The bike has no direct fuel-level signal on CAN, so the display **estimates** it:

- Tap **Reset** after every fill-up to set the tank to full (13 L).
- While you ride, the display subtracts the fuel the engine reports using (read over CAN using UDS).
- The consumed fuel and distance are saved to flash when the engine stops, so the estimate survives key-off and power loss.
- **ECON** and **RANGE** are derived from that fuel use and distance.

Refuel the motorcycle and press **Reset** every time you refuel; otherwise the gauge will be inaccurate. Always fill the tank all the way full. On the Android app, the Reset button works the same way (it is disabled while not connected).

### 8.2 Off-road ABS

Toggle **Off-road ABS** to have the bike start in off-road ABS mode at the next key-on.

1. Turn the switch **ON**. A message tells you the mode takes effect on the next key cycle.
2. Turn the ignition **OFF**, wait a few seconds, then key back **ON**.
3. The display should show **ABS: OFF-Road**.
4. To return to normal ABS, turn the switch **OFF** and key-cycle again.

How it works and safety rules:

- The setting is saved on the ESP32 and is remembered.
- At key-on the display sends the off-road request on the CAN bus. It **only transmits while the bike is stopped**.
- It **will not transmit if another module (such as a Veridian cruise control or KTM ABS dongle) is already sending the request.** It listens first and stops immediately if another sender appears.
- It stops sending if the bike moves.
- The switch also works from the Android app (when connected). The display and app stay in sync.

### 8.3 Setting the date and time (display build)

Tap the date/time on the display. A **Set date & time** panel appears with +/− steppers for year, month, day, hour and minute. Tap **Set** to save to the real-time clock, or **Cancel**. An "Invalid date or time" message appears if the values are not a real date. The clock requires a coin cell backup battery installed on the board: it is needed to keep the ESP32 clock running while power is off, and the data logs depend on that clock for their timestamps and file names. Without the battery the clock must be set again after each power loss.

The Android app does not show the clock.

### 8.4 About / version information (display build)

Tap the **CAN** label (bottom right). The **About** dialog shows the firmware version and build, and the BLE protocol version. Tap the close button to dismiss.

## 9. Android app settings

Tap the **gear icon** at the top right to open **Settings**:

| Setting | Description |
|---|---|
| **Keep display ON while using** | Keeps the phone/tablet screen awake while the app is open. |
| **Show fuel range over other apps** | Shows a small floating fuel-range window while the app is in the background (for example while using navigation). Android will ask for the "display over other apps" permission the first time. |
| **About** | Opens a dialog listing the app version, the BLE protocol range the app supports, and, when connected, the display's firmware version, build (`43b` or `headless`) and BLE protocol version. |
| **701 display to connect to** | Choose **Any (first one found)** or a specific display (`Husky701-XXXXXX`, with address and signal strength). The choice is remembered. A saved display that is out of range is shown as "Not in range". |

Use this app as the only display for the **headless** build, and as the place to read its versions.

## 10. Versions and compatibility

Three versions are tracked so the parts work together:

- **Firmware version** (for example `1.0.0`), the same number for both builds, with a build tag of `43b` or `headless`.
- **BLE protocol version** (currently **2**), bumped whenever the Bluetooth interface changes. History is in [Docs/BLE_Protocol.md](Docs/BLE_Protocol.md).
- **App version** (for example `1.1.0+5`).

The app accepts protocol versions 1 to 2. If the firmware reports a protocol version the app does not support, update the app (or the firmware) so the versions match. Always check the **About** dialogs after updating either side.

## 11. SD card logging (display build)

The display build can write data and error logs to a microSD card:

- On start, it looks for a card. If none is found **after about 8 seconds, SD logging is switched off until the next reboot**. The display and BLE keep working normally.
- With a card: CAN data files named `CANMMDDYYYY_HHMM.csv` (one per run, using the clock) and an `error.txt` log.
- **The coin cell battery is required** for both the ESP32 clock and the data logs. The logs get their timestamps and file names from the clock, so install the battery and set the clock first. Logging waits for a valid time.
- If you do not want data logging, simply leave the SD card out.
- If you insert a card later, reboot to enable logging.

The headless build has no SD logging.

## 12. Troubleshooting

| Symptom | Check |
|---|---|
| `CAN` stays yellow | Key is ON? CAN High/Low wired correctly and not swapped? ABS ECU working? Power and ground solid? |
| App says searching, never connects | Bluetooth on? Nearby-devices permission granted? ESP32 powered (key ON)? Close other apps connected to the display (only one BLE client at a time). |
| Wrong display chosen | Settings → "701 display to connect to" → pick the right `Husky701-XXXXXX`. |
| Fuel gauge wrong | Press **Reset** only at a full tank. Re-fill and reset to recalibrate. |
| Off-road ABS does not engage | Turn the switch on, then key off, wait a few seconds, and then key back on. The bike must be stopped. If a KTM ABS dongle or Veridian is already sending the request, the display deliberately does not send; enable it through the other device instead. |
| ABS shows ON after switching ABS off with the button | Expected, see the ABS note in section 7. |
| No SD logs | Card inserted before power-up? Reboot with the card in. Clock set? |
| Time is wrong | Tap the date/time on the display and set it. |
| Version mismatch | Compare the app and display **About** dialogs. Update whichever is older. |

For more detail, see the [README](README.md), the [BLE protocol](Docs/BLE_Protocol.md) and the [changelog](changelog.md).
