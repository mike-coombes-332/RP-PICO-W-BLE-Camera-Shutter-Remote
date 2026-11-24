RP-PICO-W-BLE-Camera-Shutter-Remote

The RP PICO W uses the CYW43439 WIFI/BT chipset to get wireless features from the RP2040 Microcontroller.

This project uses a BLE conection with the phone (Android was tested, and the device was paired prior to testing)

NOTE I have a wirelink from TP6 (BOOT Button) to GPIO6
<img width="954" height="703" alt="TP6_GPIO6" src="https://github.com/user-attachments/assets/99ac4807-4b76-4e0a-88e6-7f2704709820" />

When the button [BOOT] is pressed, read by GPIO6. It will send "Volume Increment" if the device is connected via BLE. 

It the camera app is open, the volume inc. will trigger a photo capture.
