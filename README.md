# Project Overview

This project uses an ESP32-S3 4.3inch Capacitive Touch LCD Development Board Type B with Case 800×480 Solution IPS Display with 2.4GHz WiFi and BLE 5 Support,32-bit LX7 Dual-core Processor as a secondary display for a Husqvarna 701 Enduro (Euro 5) motorcycle. Mine is a 2023 USA version. **The main feature of this project is the virtual fuel gauge, fuel economy, and fuel range display**. This display adds a gear indicator, RPM bar graph and display, engine coolant temperature, fuel range and fuel economy. It estimates remaining fuel by a manual reset at fill-up and then subtracting off the fuel consumed by the engine which it reads from CAN using UDS. 

<img width="798" height="477" alt="image" src="https://github.com/user-attachments/assets/3367fc76-76ad-4862-9556-7ac19bd87313" />

## CAN Hardware

I used this hardware: [https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B](https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B) to get started. However, longer tem this project will likely go to headless hardware and use mobile apps for the display. 

I used Visual Studio Code for an IDE: [https://code.visualstudio.com/](https://code.visualstudio.com/) 

More documentation in this blog post: [https://jimroal.blogspot.com/2026/10/husqvarna-701-can-touch-display.html](https://jimroal.blogspot.com/2026/10/husqvarna-701-can-touch-display.html) 

I made a T harness with a plug and socket for the Euro 5 motorcycle OBD port. This allows other devices such as Veridian cruise control or an OBD scan tool to still be connected. I use power, ground, and the CAN wires to connect to the ESP32 hardware. The ESP32 reads CAN data that is used by the display. It requires data from both the engine ECU and the ABS ECU. This system will not work without the ABS ECU operating properly. The image below shows the Euro 5 OBD port on the motorcycle, looking in at the face of the connector.

<img width="986" height="736" alt="HuskyOBDFace" src="https://github.com/user-attachments/assets/7b9ec2c9-35ec-4e07-a771-2fb836d68805" />

I used 5-pin ebike connectors at the display end of the wiring harness so I could remove the display without removing the wire harness.

You can run the ESP32 installer here: [https://jroal.github.io/701display/installer.html](https://jroal.github.io/701display/installer.html) 

### Firmware builds

- `waveshare_esp32s3_43b` builds the existing display hardware ( [https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B](https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B) ).
- `waveshare_esp32s3_rs485_can` builds the headless [Waveshare ESP32-S3-RS485-CAN](https://www.waveshare.com/esp32-s3-rs485-can.htm?sku=32154) board. It keeps CAN telemetry, BLE, fuel estimation, and flash persistence, but does not initialize the display, touch controller, I2C RTC, or SD logging. PlatformIO auto-detects its serial port.

In VS Code, run **Terminal → Run Task → Build Headless Hardware** to build the headless variant, or **Upload Headless Hardware** to flash it.

## Android Display Application

I struggled to find a single robust display that had CAN for this project. It was going to require a custom device and without committed volumes this was not a viable option. I decided to make an Android application for the display since there are already robust motorcycle tablets available on the market ([Carpuride Plus series](https://carpuride.com/collections/motorcycle-plus-series-app-enabled), Topnavi, [HugeRock](https://hugerockglobal.com/), etc.). The Android tablet or phone must have the Google Play Store on it so this app can be installed. If it does not have the Google Play Store, it would need to allow side-loading the app. This way the display becomes an app on that already existing hardware. The ESP32 BLE server broadcasts the data at 20Hz which is plenty fast for a display update. I included the Android (and placeholders for Linux and iOS) app in this same repo so I could easily manage interface changes. The app is in the APK subfolder of this repo.

October 21, 2026: Added Flutter project to develop mobile apps for receiving the data via BLE. The ESP32 will be the BLE server. Mobile apps will receive this BLE data for display on phones or tablets. I only created the basic framework so far (empty mobile apps). The goal is to broadcast this data via BLE so motorcycle tablets or phones can receive and display it. If this becomes prime path for the display of the data, I will get a smaller ESP32 with no display. For now the Waveshare ESP32-S3 display will perform this function. In order for a smaller non-display device to perform this function a fuel reset and time setting feature will need to be added to the mobile app(s). 

October 22, 2026: Android app and BLE server working. Here is a video playlist showing the system: [https://www.youtube.com/playlist?list=PLWVnSqkvN5Vg](https://www.youtube.com/playlist?list=PLWVnSqkvN5Vg) Google Play Store Listing: [https://play.google.com/store/apps/details?id=com.jroal.mobile_701_display](https://play.google.com/store/apps/details?id=com.jroal.mobile_701_display) (internal testers only, not yet open to public).

October 24, 2026: Added setting features. Created setting to keep display ON while the app is open. Added setting to select which BLE server to connect to. This handles the situation where 2 or more BLE servers are within range. By default it looks for any available. Then it lists the servers found so the user can select which one they connect to.

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

