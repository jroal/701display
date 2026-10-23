# Project Overview

This project uses an ESP32-S3 4.3inch Capacitive Touch LCD Development Board Type B with Case 800×480 Solution IPS Display with 2.4GHz WiFi and BLE 5 Support,32-bit LX7 Dual-core Processor as a secondary display for a Husqvarna 701 Enduro (Euro 5) motorcycle. Mine is a 2023 USA version. **The main feature of this project is the virtual fuel gauge, fuel economy, and fuel range display**. This display adds a gear indicator, RPM bar graph and display, engine coolant temperature, fuel range and fuel economy. It estimates remaining fuel by a manual reset at fill-up and then subtracting off the fuel consumed by the engine which it reads from CAN using UDS. 

<img width="798" height="477" alt="image" src="https://github.com/user-attachments/assets/3367fc76-76ad-4862-9556-7ac19bd87313" />

## CAN Hardware

I used this hardware: [https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B}(https://docs.waveshare.com/ESP32-S3-Touch-LCD-4.3B) to get started. However, longer tem this project will likely go to headless hardware and use mobile apps for the display. 

I used Visual Studio Code for an IDE: [https://code.visualstudio.com/](https://code.visualstudio.com/) 

More documentation in this blog post: [https://jimroal.blogspot.com/2026/10/husqvarna-701-can-touch-display.html](https://jimroal.blogspot.com/2026/10/husqvarna-701-can-touch-display.html) 

I made a T harness with a plug and socket for the Euro 5 motorcycle OBD port. This allows other devices such as Veridian cruise control or an OBD scan tool to still be connected. I use power, ground, and the CAN wires to connect to the ESP32 hardware. The ESP32 reads CAN data that is used by the display. It requires data from both the engine ECU and the ABS ECU. This system will not work without the ABS ECU operating properly. 

## Android Display Application

I struggled to find a single robust display that had CAN for this project. It was going to require a custom device and without committed volumes this was not a viable option. I decided to make an Android application for the display since there are already robust motorcycle tablets available on the market (Topnavi, HardRock, etc.). The Android tablet or phone must have the Google Play Store on it so this app can be installed. If it does not have the Google Play Store, it would need to allow side-loading the app. This way the display becomes an app on that already existing hardware. The ESP32 BLE server broadcasts the data at 20Hz which is plenty fast for a display update. I included the Android (and placeholders for Linux and iOS) app in this same repo so I could easily manage interface changes. The app is in the APK subfolder of this repo.

October 21, 2026: Added Flutter project to develop mobile apps for receiving the data via BLE. The ESP32 will be the BLE server. Mobile apps will receive this BLE data for display on phones or tablets. I only created the basic framework so far (empty mobile apps). The goal is to broadcast this data via BLE so motorcycle tablets or phones can receive and display it. If this becomes prime path for the display of the data, I will get a smaller ESP32 with no display. For now the Waveshare ESP32-S3 display will perform this function. In order for a smaller non-display device to perform this function a fuel reset and time setting feature will need to be added to the mobile app(s). 

October 22, 2026: Android app and BLE server working.
