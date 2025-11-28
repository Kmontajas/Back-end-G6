#include "trash_classifier_model_data.h" 

#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

#include "C:\Users\JC\Documents\Arduino\libraries\TensorFlowLite_ESP32\src\tensorflow\lite\micro\all_ops_resolver.h"
#include "tensorflow\lite\micro\micro_interpreter.h"
#include "tensorflow\lite\schema\schema_generated.h"
#include "C:\Users\JC\Documents\Arduino\libraries\TensorFlowLite_ESP32\src\tensorflow\lite\version.h"

// CONFIG
#define WIFI_SSID "Joasher"
#define WIFI_PASSWORD "01051014"
const char* FIREBASE_ROOT = "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app";
const char* FIREBASE_AUTH = "KqVUe0QRhh9dbCBhRhBfCuqDfLVE5suR0xXWO0yF"; 
  
const int IMG_W = 64;
const int IMG_H = 64;
const int IMG_CH = 3;

#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

constexpr int kTensorArenaSize = 20 * 1024;
static uint8_t tensor_arena[kTensorArenaSize];

using namespace tflite;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input_tensor = nullptr;

void setupCamera() {
  camera_config_t config;
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565; // we'll convert
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Camera init failed");
    while (true) delay(1000);
  }
}

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  Serial.println("\nWiFi connected");

  setupCamera();

  // TFLM init
    model = tflite::GetModel(g_model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println("Model schema mismatch!");
    while(1);
  }
  static tflite::MicroMutableOpResolver<10> resolver;
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddReshape();
    resolver.AddSoftmax();
  
  static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;
  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors() failed");
    while(1);
  }
  input_tensor = interpreter->input(0);
  Serial.println("TFLM ready");
}

void firebasePutString(const String &pathJson, const String &jsonValue) {
  HTTPClient http;
  String url = String(FIREBASE_ROOT) + pathJson;
  if (strlen(FIREBASE_AUTH) > 0) url += String(FIREBASE_AUTH);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(jsonValue);
  String resp = http.getString();
  Serial.printf("FB PUT code=%d resp=%s\n", code, resp.c_str());
  http.end();
}

// simple nearest-neighbor resize & normalize into tensor (RGB)
void preprocess_rgb565_to_uint8(camera_fb_t *fb) {
  // assume input tensor is uint8 and expects [H,W,3]
  uint8_t *out = input_tensor->data.uint8;
  int srcW = fb->width;
  int srcH = fb->height;
  int dstW = IMG_W, dstH = IMG_H;
  uint8_t *src = fb->buf;
  for (int y=0; y<dstH; y++) {
    int sy = (y * srcH) / dstH;
    for (int x=0; x<dstW; x++) {
      int sx = (x * srcW) / dstW;
      int idx = (sy * srcW + sx) * 2;
      if (idx+1 >= fb->len) {
        out[(y*dstW + x)*3 + 0] = 128;
        out[(y*dstW + x)*3 + 1] = 128;
        out[(y*dstW + x)*3 + 2] = 128;
        continue;
      }
      uint8_t b1 = src[idx];
      uint8_t b2 = src[idx+1];
      uint16_t pix = (uint16_t)b2 << 8 | b1;
      uint8_t r = (pix >> 11) & 0x1F; r = (r * 255) / 31;
      uint8_t g = (pix >> 5) & 0x3F;  g = (g * 255) / 63;
      uint8_t b = pix & 0x1F;         b = (b * 255) / 31;
      out[(y*dstW + x)*3 + 0] = r;
      out[(y*dstW + x)*3 + 1] = g;
      out[(y*dstW + x)*3 + 2] = b;
    }
  }
}

const char* CLASS_LABELS[] = {"biodegradable","non_biodegradable","recyclable"};

void loop() {
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) { delay(2000); return; }

  // preprocess
  if (input_tensor->type == kTfLiteUInt8) {
    preprocess_rgb565_to_uint8(fb);
  } else {
    Serial.println("Model input type not supported in example");
  }

  // run inference
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("Invoke failed");
    esp_camera_fb_return(fb);
    delay(1000);
    return;
  }

  // read output probabilities
  TfLiteTensor* output = interpreter->output(0);
  int best = 0;
  float bestscore = -10000.0f;
  if (output->type == kTfLiteInt8) {
    // dequantize
    for (int i=0;i<output->bytes;i++) {
      int8_t v = output->data.int8[i];
      float val = (v - output->params.zero_point) * output->params.scale;
      if (val > bestscore) { bestscore = val; best = i; }
    }
  } else if (output->type == kTfLiteUInt8) {
    for (int i=0;i<output->bytes;i++) {
      uint8_t v = output->data.uint8[i];
      float val = (v - output->params.zero_point) * output->params.scale;
      if (val > bestscore) { bestscore = val; best = i; }
    }
  } else if (output->type == kTfLiteFloat32) {
    int N = output->bytes / sizeof(float);
    float* arr = output->data.f;
    for (int i=0;i<N;i++) {
      if (arr[i] > bestscore) { bestscore = arr[i]; best = i; }
    }
  }

  String raw = CLASS_LABELS[best];
  Serial.printf("Raw: %s (score %.3f)\n", raw.c_str(), bestscore);

  // Map raw -> category
  String category = "non-biodegradable";
  raw.toLowerCase();
  if (raw.indexOf("plastic") >= 0 || raw.indexOf("metal") >= 0) category = "recyclable";
  else if (raw.indexOf("food") >= 0 || raw.indexOf("paper") >= 0) category = "biodegradable";

  Serial.println("Category: " + category);

  // Upload to Firebase (put JSON string)
  String path = "/classification/latest.json";
  String json = "\"" + category + "\"";
  firebasePutString(path, json);

  esp_camera_fb_return(fb);
  delay(2500); 
}
