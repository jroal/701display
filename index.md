# 701 Display Project

# Project Overview

This project uses an ESP32-S3 module (either a 4.3inch Capacitive Touch LCD Development Board Type B with Case 800×480 Solution IPS Display with 2.4GHz WiFi and BLE 5 Support,32-bit LX7 Dual-core Processor (or a headless ESP32 as a secondary display for a Husqvarna 701 Enduro (Euro 5) motorcycle. Mine is a 2023 USA version. This should also work on other KTM LC4 equipped motorcycles (KTM 690, GasGas 700) with the Bosch 9.1 MP CAN bus network / Keihin ECU platform and Euro 5 OBD connector:

- GasGas 700: Every model year made (2022+). (needs testing)
- KTM 690: 2021 and newer. (needs testing)
- Husqvarna 701: 2021 and newer. (developed and tested on this)   

**The main feature of this project is the virtual fuel gauge, fuel economy, and fuel range display**. This display adds a gear indicator, RPM bar graph and display, engine coolant temperature, fuel range and fuel economy. It estimates remaining fuel by a manual reset at fill-up and then subtracting off the fuel consumed by the engine which it reads from CAN using UDS. 

In November 2026 an ABS off-road mode feature was added. This feature allows the user to enable off-road mode which becomes active on the next key cycle. This performs the same function as the off-road ABS dongle but allows the user to enable/disable it as needed. 

<img width="799" height="476" alt="ESP32_43b_Emulator" src="https://github.com/user-attachments/assets/a35310a3-be32-482c-b272-e6d41821409f" />


See the owners manual here [https://jroal.github.io/701display/OWNERS_MANUAL](https://jroal.github.io/701display/OWNERS_MANUAL)

## CAN Hardware

I used this hardware: [https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B](https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B) to get started and for development. This allowed me to capture CAN data during development. Later I developed a headless version with this [https://www.waveshare.com/product/esp32-related/industrial-controller/esp32-s3-rs485-can.htm](https://www.waveshare.com/product/esp32-related/industrial-controller/esp32-s3-rs485-can.htm) headless hardware and use mobile apps for the display. There is an Android app in this same repo.

I used Visual Studio Code for an IDE: [https://code.visualstudio.com/](https://code.visualstudio.com/) 

More documentation in this blog post: [https://jimroal.blogspot.com/2026/10/husqvarna-701-can-touch-display.html](https://jimroal.blogspot.com/2026/10/husqvarna-701-can-touch-display.html). I created that post when I was just starting the project. 

I made a T harness with a plug and socket for the Euro 5 motorcycle OBD port. This allows other devices such as Veridian cruise control or an OBD scan tool to still be connected. I use power, ground, and the CAN wires to connect to the ESP32 hardware. The ESP32 reads CAN data that is used by the display. It requires data from both the engine ECU and the ABS ECU. This system will not work properly without the ABS ECU operating properly. 

See the [owners manual](https://jroal.github.io/701display/OWNERS_MANUAL) for wiring details.

These are [sealed electrical connectors 6189-6171 6180-6181 Compatible with Sumitomo HM 090](https://www.amazon.com/dp/B0FCLJP2ZY).

I used 5-pin ebike connectors at the display end of the wiring harness for the ESP32 4.3b so I could remove the display without removing the wire harness. The ESP32 display version is really for development only as it is not robust enough for long-term use on the motorcycle. Use the headless ESP32 hardware and seal it from moisture. Then use hardened motorcycle Android tablet or phone for the user interface. 

You can run the ESP32 installer here: [https://jroal.github.io/701display/installer.html](https://jroal.github.io/701display/installer.html) 

### Firmware builds

- `waveshare_esp32s3_43b` builds the ESP32-S3 for the display version hardware ( [https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B](https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B) ).
- `waveshare_esp32s3_rs485_can` builds the headless [Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/esp32-s3-rs485-can.htm?sku=32154) board. It keeps all the features except it does not support an on-board display, touch controller, I2C RTC, or SD logging. 

In VS Code, run **Terminal → Run Task → Build Headless Hardware** to build the headless variant, or **Upload Headless Hardware** to flash it. PlatformIO auto-detects its serial port.

## Android Display Application

I struggled to find a single robust display that had CAN for this project. It was going to require a custom device and without committed volumes this was not a viable option. I decided to make an Android application for the display since there are already robust motorcycle tablets available on the market ([Carpuride Plus series](https://carpuride.com/collections/motorcycle-plus-series-app-enabled), Topnavi, [HugeRock](https://hugerockglobal.com/), etc.). The Android tablet or phone must have the Google Play Store on it so this app can be installed. If it does not have the Google Play Store, it would need to allow side-loading the app. This way the display becomes an app on that already existing hardware. The ESP32 BLE server broadcasts the data at 20Hz which is plenty fast for a display update. I included the Android (and placeholders for Linux and iOS) app in this same repo so I could easily manage interface changes. The app is in the APK subfolder of this repo. We have now tested using the headless hardware and a [Carpuride W702 Plus motorcycle tablet](https://carpuride.com/collections/motorcycle-plus-series-app-enabled/products/carpuride-w702s-plus-wireless-portable-motorcycle-stereo-plug-and-play-bm05-bracket-install-apps-built-in-gps-dual-cameras).  

<img alt="image" src="https://github.com/user-attachments/assets/f4ee32e8-76d1-41d0-8354-238553d24424" />


Google Play Store Listing: [https://play.google.com/store/apps/details?id=com.jroal.mobile_701_display](https://play.google.com/store/apps/details?id=com.jroal.mobile_701_display) 

The changelog is here: [https://jroal.github.io/701display/changelog.html](https://jroal.github.io/701display/changelog.html)

# DISCLAIMER OF WARRANTY & LIMITATION OF LIABILITY

IMPORTANT: READ BEFORE USING THIS HARDWARE, FIRMWARE, OR APPLICATION.

1. Provided "As-Is" Without Warranty

    The Husky Display project—including all hardware schematics, firmware binaries, source code, Android applications, and documentation—is provided strictly on an "AS IS" and "AS AVAILABLE" basis, without warranties or conditions of any kind, either express or implied. This includes, but is not limited to, implied warranties of merchantability, fitness for a particular purpose, accuracy, or non-infringement.

2. Informational Use Only & Primary Instrumentation

    - Not an OEM Replacement: This system is designed solely as an experimental, secondary telemetry aid and virtual fuel computer. It does NOT replace your motorcycle’s original factory instrumentation, speedometer, low-fuel warning light, or primary diagnostic displays.

    - Telemetry Accuracy: All calculated metrics—including but not limited to vehicle speed, engine RPM, remaining fuel volume, fuel consumption rates, estimated range, and gear selection—are approximations derived from empirical data and CAN bus telemetry. Do not rely on this product for critical navigation, speed limit compliance, or fuel management. Always monitor your motorcycle’s factory dash and physical fuel level.

3. Rider Responsibility & Traffic Safety

    - Safe Operation: The operator of the motorcycle assumes sole responsibility for safe riding, attention to road conditions, and adherence to all local traffic laws.

    - Distraction Hazard: Interacting with, configuring, or looking at display screens, phones, or handlebar mounts while riding can cause severe distraction, accidents, bodily injury, or death. Never adjust settings or attempt to operate the application while the motorcycle is in motion.

4. Vehicle Electrical & Engine Interfacing

    - User Assumption of Risk: Connecting third-party devices or custom T-harnesses to your motorcycle's diagnostic (OBD2/Euro 5) CAN bus carries inherent risks, including potential electrical short circuits, battery drain, ECU communication faults, or voiding of vehicle manufacturer warranties.

    - No Liability for Vehicle Damage: The developers, contributors, and copyright holders of this project shall not be held liable for any damage to your motorcycle, electronic control units (ECU), electrical harness, or connected peripherals resulting from the installation or use of this hardware or software.

5. Limitation of Liability

    To the maximum extent permitted by applicable law, in no event shall the authors, developers, or project contributors be liable for any direct, indirect, incidental, special, exemplary, punitive, or consequential damages (including, but not limited to, loss of use, personal injury, wrongful death, property damage, vehicle repair costs, or business interruption) however caused and on any theory of liability, whether in contract, strict liability, or tort (including negligence or otherwise) arising in any way out of the use of this product, even if advised of the possibility of such damage.

