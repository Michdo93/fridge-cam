#include "esp_camera.h"
#include <WiFi.h>
#include "esp_timer.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_http_server.h"

// -----------------------------------------------------------------------------
// Configuration — edit these values
// -----------------------------------------------------------------------------
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// -----------------------------------------------------------------------------
// AI-Thinker ESP32-CAM pin mapping (AZDelivery ESP32-CAM)
// -----------------------------------------------------------------------------
#define PWDN_GPIO_NUM  32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM   0
#define SIOD_GPIO_NUM  26
#define SIOC_GPIO_NUM  27
#define Y9_GPIO_NUM    35
#define Y8_GPIO_NUM    34
#define Y7_GPIO_NUM    39
#define Y6_GPIO_NUM    36
#define Y5_GPIO_NUM    21
#define Y4_GPIO_NUM    19
#define Y3_GPIO_NUM    18
#define Y2_GPIO_NUM     5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM  23
#define PCLK_GPIO_NUM  22
#define FLASH_GPIO_NUM  4

// -----------------------------------------------------------------------------
// Globals
// -----------------------------------------------------------------------------
static uint8_t* lastPhotoBuffer = nullptr;
static size_t   lastPhotoSize   = 0;
static String   lastPhotoTime   = "no photo yet";

httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

#define PART_BOUNDARY "frame"
static const char* STREAM_CONTENT_TYPE =
    "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART =
    "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// -----------------------------------------------------------------------------
// Camera init
// -----------------------------------------------------------------------------
void initCamera() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 10;
    config.fb_count     = 2;
    config.grab_mode    = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size   = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed — restarting");
    delay(1000);
    ESP.restart();
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_whitebal(s, 1);
  s->set_awb_gain(s, 1);
  s->set_exposure_ctrl(s, 1);
  s->set_gain_ctrl(s, 1);
  s->set_raw_gma(s, 1);
  s->set_lenc(s, 1);
  s->set_wpc(s, 1);
  s->set_dcw(s, 1);

  // warm-up frames
  for (int i = 0; i < 2; i++) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (fb) esp_camera_fb_return(fb);
  }
  Serial.println("Camera ready");
}

// -----------------------------------------------------------------------------
// HTTP handlers
// -----------------------------------------------------------------------------
static esp_err_t stream_handler(httpd_req_t* req) {
  camera_fb_t* fb = NULL;
  char part_buf[64];

  httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Framerate", "60");
  Serial.println("Stream started");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) break;

    if (httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY)) != ESP_OK) break;
    size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, fb->len);
    if (httpd_resp_send_chunk(req, part_buf, hlen) != ESP_OK) break;
    if (httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len) != ESP_OK) break;

    esp_camera_fb_return(fb);
    fb = NULL;
  }

  if (fb) esp_camera_fb_return(fb);
  Serial.println("Stream ended");
  return ESP_OK;
}

static esp_err_t photo_handler(httpd_req_t* req) {
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, HIGH);
  delay(50);
  camera_fb_t* fb = esp_camera_fb_get();
  digitalWrite(FLASH_GPIO_NUM, LOW);

  if (!fb) { httpd_resp_send_500(req); return ESP_FAIL; }

  if (lastPhotoBuffer) free(lastPhotoBuffer);
  lastPhotoBuffer = (uint8_t*)malloc(fb->len);
  if (lastPhotoBuffer) {
    memcpy(lastPhotoBuffer, fb->buf, fb->len);
    lastPhotoSize = fb->len;
    lastPhotoTime = String(millis() / 1000) + "s uptime";
  }

  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=photo.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  Serial.printf("Photo taken: %d bytes\n", lastPhotoSize);
  return res;
}

static esp_err_t last_handler(httpd_req_t* req) {
  if (!lastPhotoBuffer || lastPhotoSize == 0) {
    httpd_resp_send_404(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=last.jpg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "X-Photo-Time", lastPhotoTime.c_str());
  return httpd_resp_send(req, (const char*)lastPhotoBuffer, lastPhotoSize);
}

static esp_err_t status_handler(httpd_req_t* req) {
  String json = "{";
  json += "\"lastPhoto\":\"" + lastPhotoTime + "\",";
  json += "\"lastPhotoSize\":" + String(lastPhotoSize) + ",";
  json += "\"freeHeap\":" + String(ESP.getFreeHeap()) + ",";
  json += "\"psram\":" + String(psramFound() ? "true" : "false") + ",";
  json += "\"uptime\":" + String(millis() / 1000);
  json += "}";
  httpd_resp_set_type(req, "application/json");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  return httpd_resp_sendstr(req, json.c_str());
}

// -----------------------------------------------------------------------------
// Server startup
// -----------------------------------------------------------------------------
void startServers() {
  // Stream server on port 81
  httpd_config_t stream_config = HTTPD_DEFAULT_CONFIG();
  stream_config.server_port  = 81;
  stream_config.ctrl_port    = 32769;
  stream_config.max_uri_handlers = 2;

  httpd_uri_t stream_uri = {
    .uri = "/stream", .method = HTTP_GET,
    .handler = stream_handler, .user_ctx = NULL
  };
  if (httpd_start(&stream_httpd, &stream_config) == ESP_OK)
    httpd_register_uri_handler(stream_httpd, &stream_uri);

  // API server on port 80
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_uri_handlers = 8;

  httpd_uri_t uris[] = {
    { .uri = "/photo",  .method = HTTP_GET, .handler = photo_handler,  .user_ctx = NULL },
    { .uri = "/last",   .method = HTTP_GET, .handler = last_handler,   .user_ctx = NULL },
    { .uri = "/status", .method = HTTP_GET, .handler = status_handler, .user_ctx = NULL },
  };

  if (httpd_start(&camera_httpd, &config) == ESP_OK)
    for (auto& u : uris)
      httpd_register_uri_handler(camera_httpd, &u);
}

// -----------------------------------------------------------------------------
// Setup & loop
// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  initCamera();

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }

  String ip = WiFi.localIP().toString();
  Serial.println("\nConnected! IP: " + ip);
  Serial.println("Endpoints:");
  Serial.println("  GET http://" + ip + "/photo");
  Serial.println("  GET http://" + ip + "/last");
  Serial.println("  GET http://" + ip + "/status");
  Serial.println("  GET http://" + ip + ":81/stream");

  startServers();
}

void loop() {
  delay(10000);
}
