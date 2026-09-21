#pragma once

// Copy this file to config.h and fill in your home Wi-Fi.
// If station mode fails, the board starts a hotspot instead.

#define WIFI_SSID        "YourWiFiName"
#define WIFI_PASSWORD    "YourWiFiPassword"

#define AP_SSID          "ESP-CAM"
#define AP_PASSWORD      "esp32cam"
#define WIFI_STA_TIMEOUT_MS 10000

#define SERVO_PIN        14
#define SERVO_MIN_US     500
#define SERVO_MAX_US     2400
#define SERVO_MIN_ANGLE  0
#define SERVO_MAX_ANGLE  180
#define SERVO_HOME_ANGLE 90

#define TRIG_PIN         13
#define ECHO_PIN         15
#define US_TIMEOUT_US    30000

#define CAM_FRAME_SIZE   FRAMESIZE_QVGA
#define CAM_JPEG_QUALITY 12

#define MOTION_ON_CM         170.0f
#define MOTION_SLEEP_CM      250.0f
#define MOTION_SLEEP_MS      4000
#define MOTION_COOLDOWN_MS   2500
#define MOTION_POLL_MS       150
#define SERVO_IGNORE_MS      800
#define EVENT_LOG_SIZE       40
