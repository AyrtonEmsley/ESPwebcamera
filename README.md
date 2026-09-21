# ESP32-CAM Phone Webcam

Live MJPEG from an ESP32-WROVER camera board, pan from your phone, and an HC-SR04 distance readout. The board serves the website itself.

It tries your home Wi-Fi first. If that fails, it starts a hotspot named `ESP-CAM` (password `esp32cam`) at `http://192.168.4.1`.

## What you need

- ESP32-WROVER camera board (WROVER-KIT / Freenove CAM)
- SG90-style 180° servo
- HC-SR04 ultrasonic sensor
- Two resistors for a 5V→3.3V divider (2.2 kΩ and 3.3 kΩ, or similar)
- Breadboard and jumper wires
- USB–UART adapter (FTDI / CP2102 / CH340)
- **5V / 2A+** supply — do not power the servo from the ESP32 3.3V pin or from the UART adapter alone

## Pin map

The camera already uses most GPIOs. Only leftover pins are used here:

| Function | GPIO |
| --- | --- |
| Servo signal | GPIO 14 |
| Ultrasonic TRIG | GPIO 13 |
| Ultrasonic ECHO | GPIO 15 (via divider) |
| Common ground | GND |
| Board power | 5V |
| Servo VCC | external 5V (same rail as the board 5V) |

```
                  5V / 2A+ supply
                   |         |
                   |         +---- servo VCC (red)
                   +---- ESP32-CAM 5V
                   |
                  GND -------- ESP32-CAM GND
                   |           servo GND (brown/black)
                   |           HC-SR04 GND
                   |
HC-SR04 VCC ------ 5V
HC-SR04 TRIG ----- GPIO 13
HC-SR04 ECHO --2.2k--+-- GPIO 15
                     |
                    3.3k
                     |
                    GND

Servo signal (orange/yellow) ---- GPIO 14
```

ECHO on a standard HC-SR04 is 5V. The divider drops it to about 3.0V so the ESP32 input stays safe. TRIG is fine at 3.3V from the ESP32.

## Flash (first time)

1. Copy [`include/config.example.h`](include/config.example.h) to `include/config.h` and set `WIFI_SSID` / `WIFI_PASSWORD`.
2. UART adapter: **U0R → TX**, **U0T → RX**, **GND → GND**. Power the board from the 5V supply (or adapter 5V if it can supply enough current for flashing).
3. Put **GPIO 0 to GND** (programming mode).
4. Press the board **RESET** button.
5. Upload:

```bash
pio run -t upload
```

6. Remove the GPIO 0 jumper, press RESET again, then open the Serial monitor at **115200** baud. It prints the IP (home Wi-Fi or hotspot).
7. On your phone, open `http://THE_IP/` (same Wi-Fi, or join the `ESP-CAM` hotspot first).

If upload fails, keep GPIO 0 grounded, drop `upload_speed` in `platformio.ini`, and retry.

## Using the page

- Live view is an MJPEG stream (control on port 80, stream on port 81).
- Slider and ◀ ▶ pan the servo (0–180°).
- Distance updates a few times per second. `—` means no echo (timeout / out of range).

To bump the image from QVGA (320×240) to VGA, change `CAM_FRAME_SIZE` in `config.h` to `FRAMESIZE_VGA`. If the stream stutters, go back to QVGA.

## Power glitches

If the picture tears or the board resets when the servo moves, the 5V rail is sagging. Use a stronger supply and keep servo current off the UART adapter.

## Project layout

- [`platformio.ini`](platformio.ini) — ESP32-CAM Arduino / PlatformIO env
- [`src/main.cpp`](src/main.cpp) — Wi-Fi, camera, HTTP, servo, ranging
- [`src/index.html.h`](src/index.html.h) — phone UI (served from flash)
- [`include/camera_pins.h`](include/camera_pins.h) — AI-Thinker camera pins
- [`include/config.example.h`](include/config.example.h) — Wi-Fi and pin defaults
