#include <Arduino.h>
#include <WiFi.h>
#include <ESP32Servo.h>
#include "esp_camera.h"
#include "esp_http_server.h"
#include "camera_pins.h"
#include "config.h"
#include "index.html.h"

static Servo panServo;
static int currentAngle = SERVO_HOME_ANGLE;
static bool usingAp = false;
static IPAddress localIp(0, 0, 0, 0);

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static int clampAngle(int angle) {
  if (angle < SERVO_MIN_ANGLE) return SERVO_MIN_ANGLE;
  if (angle > SERVO_MAX_ANGLE) return SERVO_MAX_ANGLE;
  return angle;
}

static void setPanAngle(int angle) {
  currentAngle = clampAngle(angle);
  panServo.write(currentAngle);
}

static float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  const unsigned long duration = pulseIn(ECHO_PIN, HIGH, US_TIMEOUT_US);
  if (duration == 0) {
    return -1.0f;
  }
  return (duration * 0.0343f) / 2.0f;
}

static void jsonResponse(httpd_req_t *req, const String &body) {
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  httpd_resp_send(req, body.c_str(), body.length());
}

static esp_err_t indexHandler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t statusHandler(httpd_req_t *req) {
  String body = "{";
  body += "\"ap\":";
  body += usingAp ? "true" : "false";
  body += ",\"ip\":\"";
  body += localIp.toString();
  body += "\",\"angle\":";
  body += currentAngle;
  body += "}";
  jsonResponse(req, body);
  return ESP_OK;
}

static esp_err_t distanceHandler(httpd_req_t *req) {
  const float cm = readDistanceCm();
  String body = "{\"cm\":";
  if (cm < 0) {
    body += "null";
  } else {
    body += String(cm, 1);
  }
  body += "}";
  jsonResponse(req, body);
  return ESP_OK;
}

static esp_err_t controlHandler(httpd_req_t *req) {
  char query[64] = {0};
  char value[16] = {0};
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "angle", value, sizeof(value)) == ESP_OK) {
      setPanAngle(atoi(value));
    }
  }

  String body = "{\"angle\":";
  body += currentAngle;
  body += "}";
  jsonResponse(req, body);
  return ESP_OK;
}

static esp_err_t streamHandler(httpd_req_t *req) {
  camera_fb_t *fb = nullptr;
  esp_err_t res = ESP_OK;
  char partBuf[64];

  res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "15");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
      break;
    }

    const size_t hlen = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
    if (httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)) != ESP_OK ||
        httpd_resp_send_chunk(req, partBuf, hlen) != ESP_OK ||
        httpd_resp_send_chunk(req, reinterpret_cast<const char *>(fb->buf), fb->len) != ESP_OK) {
      esp_camera_fb_return(fb);
      res = ESP_FAIL;
      break;
    }
    esp_camera_fb_return(fb);
  }
  return res;
}

static bool startCamera() {
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = CAM_FRAME_SIZE;
  config.jpeg_quality = CAM_JPEG_QUALITY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (psramFound()) {
    config.fb_count = 2;
  } else {
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.frame_size = FRAMESIZE_QVGA;
  }

  const esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor && sensor->id.PID == OV3660_PID) {
    sensor->set_vflip(sensor, 1);
    sensor->set_brightness(sensor, 1);
    sensor->set_saturation(sensor, -2);
  }
  return true;
}

static void startWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting to %s", WIFI_SSID);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_STA_TIMEOUT_MS) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    usingAp = false;
    localIp = WiFi.localIP();
    Serial.print("Home Wi-Fi IP: ");
    Serial.println(localIp);
    return;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  usingAp = true;
  localIp = WiFi.softAPIP();
  Serial.printf("Hotspot %s IP: ", AP_SSID);
  Serial.println(localIp);
}

static void startServers() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.max_uri_handlers = 8;
  config.stack_size = 8192;

  httpd_uri_t indexUri = {};
  indexUri.uri = "/";
  indexUri.method = HTTP_GET;
  indexUri.handler = indexHandler;

  httpd_uri_t statusUri = {};
  statusUri.uri = "/status";
  statusUri.method = HTTP_GET;
  statusUri.handler = statusHandler;

  httpd_uri_t controlUri = {};
  controlUri.uri = "/control";
  controlUri.method = HTTP_GET;
  controlUri.handler = controlHandler;

  httpd_uri_t distanceUri = {};
  distanceUri.uri = "/distance";
  distanceUri.method = HTTP_GET;
  distanceUri.handler = distanceHandler;

  httpd_uri_t streamUri = {};
  streamUri.uri = "/stream";
  streamUri.method = HTTP_GET;
  streamUri.handler = streamHandler;

  httpd_handle_t controlHttpd = nullptr;
  if (httpd_start(&controlHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(controlHttpd, &indexUri);
    httpd_register_uri_handler(controlHttpd, &statusUri);
    httpd_register_uri_handler(controlHttpd, &controlUri);
    httpd_register_uri_handler(controlHttpd, &distanceUri);
    Serial.println("Control server on port 80");
  }

  config.server_port = 81;
  config.ctrl_port = 32769;
  httpd_handle_t streamHttpd = nullptr;
  if (httpd_start(&streamHttpd, &config) == ESP_OK) {
    httpd_register_uri_handler(streamHttpd, &streamUri);
    Serial.println("Stream server on port 81");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);
  delay(200);
  Serial.println("\nESP-CAM starting");

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  if (!startCamera()) {
    Serial.println("Continuing without a working camera stream");
  }

  // Camera XCLK uses LEDC timer 0; leave it alone for the servo.
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  panServo.setPeriodHertz(50);
  panServo.attach(SERVO_PIN, SERVO_MIN_US, SERVO_MAX_US);
  setPanAngle(SERVO_HOME_ANGLE);

  startWifi();
  startServers();

  Serial.println("Open the IP above in your phone browser");
}

void loop() {
  delay(1000);
}
