#include "best_model_latest.h"   // your model as C array
#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>

// TensorFlow Lite Micro
#include "C:\Users\Admin\Documents\Arduino\libraries\TensorFlowLite_ESP32\src\tensorflow\lite\micro\all_ops_resolver.h"
#include "tensorflow\lite\micro\micro_interpreter.h"
#include "tensorflow\lite\schema\schema_generated.h"
#include "C:\Users\Admin\Documents\Arduino\libraries\TensorFlowLite_ESP32\src\tensorflow\lite\version.h"

// Use board_config.h pin mappings (from your tested sample)
#include "board_config.h"

// ===========================
// Wi-Fi (use the credentials you gave in sample)
const char *ssid = "Joasher";
const char *password = "01051014";
  
// Firebase config (kept from your original classifier code)
const char* FIREBASE_ROOT = "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app";
const char* FIREBASE_AUTH = "KqVUe0QRhh9dbCBhRhBfCuqDfLVE5suR0xXWO0yF"; 

// Model input size (must match model)
constexpr int IMG_W = 32;
constexpr int IMG_H = 32;
constexpr int IMG_CH = 3;

// Tensor arena — increase if AllocateTensors fails (try 60*1024)
constexpr int kTensorArenaSize = 100 * 1024;
uint8_t* tensor_arena;

using namespace tflite;
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* input_tensor = nullptr;

// Labels (must match your model's class ordering)
const char* CLASS_LABELS[] = {"biodegradable","non_biodegradable","recyclable"};

// Forward declarations
void setupCamera();
void firebasePutString(const String &pathJson, const String &jsonValue);
void preprocess_rgb565_to_uint8(camera_fb_t *fb);

void preprocess_rgb565_to_float(camera_fb_t *fb) {
    float *out = input_tensor->data.f;
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
                *out++ = 0.5f; 
                *out++ = 0.5f; 
                *out++ = 0.5f; 
                continue; 
            }

            uint16_t pix = ((uint16_t)src[idx+1] << 8) | src[idx];
            uint8_t r5 = (pix >> 11) & 0x1F;
            uint8_t g6 = (pix >> 5) & 0x3F;
            uint8_t b5 = pix & 0x1F;

            float r = ((r5 * 255)/31)/255.0f;
            float g = ((g6 * 255)/63)/255.0f;
            float b = ((b5 * 255)/31)/255.0f;

            *out++ = r;
            *out++ = g;
            *out++ = b;
        }
    }
}

// Preprocess RGB565 frame into INT8 input tensor for quantized models
void preprocess_rgb565_to_int8(camera_fb_t *fb) {
    int8_t *out = input_tensor->data.int8;
    int srcW = fb->width, srcH = fb->height;
    int dstW = IMG_W, dstH = IMG_H;
    uint8_t *src = fb->buf;

    for (int y = 0; y < dstH; y++) {
        int sy = (y * srcH) / dstH;
        for (int x = 0; x < dstW; x++) {
            int sx = (x * srcW) / dstW;
            int idx = (sy * srcW + sx) * 2;
            if (idx+1 >= fb->len) { 
                *out++ = 0; *out++ = 0; *out++ = 0; 
                continue; 
            }
            uint16_t pix = ((uint16_t)src[idx+1] << 8) | src[idx];
            uint8_t r5 = (pix >> 11) & 0x1F;
            uint8_t g6 = (pix >> 5) & 0x3F;
            uint8_t b5 = pix & 0x1F;

            int8_t r = ((r5 * 255 / 31) - 128);
            int8_t g = ((g6 * 255 / 63) - 128);
            int8_t b = ((b5 * 255 / 31) - 128);

            *out++ = r;
            *out++ = g;
            *out++ = b;
        }
    }
}


uint8_t *prev_fb_buf = nullptr;
size_t prev_fb_len = 0;

bool hasFrameChanged(camera_fb_t *fb, int threshold = 30) {
    if (!prev_fb_buf) {
        // First frame, store it and report "changed"
        prev_fb_len = fb->len;
        prev_fb_buf = (uint8_t*)malloc(prev_fb_len);
        if (!prev_fb_buf) {
            Serial.println("Failed to allocate prev_fb_buf");
            return true;
        }
        memcpy(prev_fb_buf, fb->buf, prev_fb_len);
        return true;
    }

    // Compare current frame with previous
    int step = 20;  // sample every 20 bytes for speed
    long diff_sum = 0;
    size_t len = fb->len < prev_fb_len ? fb->len : prev_fb_len;

    for (size_t i = 0; i < len; i += step) {
        diff_sum += abs(fb->buf[i] - prev_fb_buf[i]);
    }

    int avg_diff = diff_sum / (len / step);

    if (avg_diff > threshold) {
        // Significant change: update prev_fb_buf
        if (fb->len != prev_fb_len) {
            // Reallocate if size changed
            free(prev_fb_buf);
            prev_fb_len = fb->len;
            prev_fb_buf = (uint8_t*)malloc(prev_fb_len);
        }
        memcpy(prev_fb_buf, fb->buf, prev_fb_len);
        return true;
    }

    // No significant change
    return false;
}


void setup() {
  Serial.begin(115200);
  delay(100);

  tensor_arena = (uint8_t*) ps_malloc(kTensorArenaSize);
  if (!tensor_arena) {
    Serial.println("Tensor arena PSRAM allocation failed!");
    while (1);
  }
  delay(100);

  Serial.println();
  Serial.println("Starting classifier-only firmware...");

  // Connect WiFi
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());

  // Initialize camera using board_config pin mapping
  setupCamera();

  // TFLM init
  model = tflite::GetModel(model_data);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.printf("Model schema mismatch: %d vs %d\n", model->version(), TFLITE_SCHEMA_VERSION);
    while (true) delay(1000);
  }

  // Add only needed ops (keep count small to reduce binary size)
// Add only needed ops (keep count small to reduce binary size)
static tflite::MicroMutableOpResolver<14> resolver;  // increase count to fit all ops

resolver.AddConv2D();
resolver.AddAveragePool2D();
resolver.AddMaxPool2D();
resolver.AddFullyConnected();
resolver.AddReshape();
resolver.AddSoftmax();
resolver.AddResizeBilinear();
resolver.AddShape();
resolver.AddStridedSlice();
resolver.AddPack();
resolver.AddQuantize();
resolver.AddMul();   // previously missing
resolver.AddAdd();   // <-- add this line


  // Create interpreter
  static tflite::MicroInterpreter static_interpreter(model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println("AllocateTensors() failed — try increasing kTensorArenaSize");
    while (true) delay(1000);
  }

  input_tensor = interpreter->input(0);
  Serial.println("TFLM ready");
   Serial.print("Model input type: ");
  switch (input_tensor->type) {
    case kTfLiteFloat32: Serial.println("FLOAT32"); break;
    case kTfLiteUInt8:   Serial.println("UINT8"); break;
    case kTfLiteInt8:    Serial.println("INT8"); break;
    default: Serial.println("UNKNOWN"); break;
  }
}


void loop() {
  // Capture frame
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    delay(2000);
    return;
  }

  if (!hasFrameChanged(fb)) {
        Serial.println("No changes detected");
        esp_camera_fb_return(fb);
        delay(500);
        return;
    }

  // Preprocess input
  if (input_tensor->type == kTfLiteUInt8) {
      preprocess_rgb565_to_uint8(fb);
  } else if (input_tensor->type == kTfLiteFloat32) {
      preprocess_rgb565_to_float(fb);
  } else if (input_tensor->type == kTfLiteInt8) {
      preprocess_rgb565_to_int8(fb); // you need this function
  } else {
      Serial.println("Model input type not supported");
      esp_camera_fb_return(fb);
      delay(2000);
      return;
  }

  // Run inference
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println("Invoke failed");
    esp_camera_fb_return(fb);
    delay(1000);
    return;
  }

   // Print model output
    TfLiteTensor* output = interpreter->output(0);
    Serial.print("=== Model Output ===\n");
    if (output->type == kTfLiteInt8) {
        for (int i = 0; i < output->bytes; i++) {
            int8_t v = output->data.int8[i];
            float val = (v - output->params.zero_point) * output->params.scale;
            Serial.printf("Output[%d] = %d -> %.3f\n", i, v, val);
        }
    } else if (output->type == kTfLiteUInt8) {
        for (int i = 0; i < output->bytes; i++) {
            uint8_t v = output->data.uint8[i];
            float val = (v - output->params.zero_point) * output->params.scale;
            Serial.printf("Output[%d] = %d -> %.3f\n", i, v, val);
        }
    } else if (output->type == kTfLiteFloat32) {
        int N = output->bytes / sizeof(float);
        for (int i = 0; i < N; i++) {
            Serial.printf("Output[%d] = %.3f\n", i, output->data.f[i]);
        }
    }

    // Determine best class
    int best = 0;
    float bestscore = -INFINITY;
    int N = (output->type == kTfLiteFloat32) ? output->bytes / sizeof(float) : output->bytes;
    for (int i = 0; i < N; i++) {
        float val = 0;
        if (output->type == kTfLiteInt8) val = (output->data.int8[i] - output->params.zero_point) * output->params.scale;
        else if (output->type == kTfLiteUInt8) val = (output->data.uint8[i] - output->params.zero_point) * output->params.scale;
        else if (output->type == kTfLiteFloat32) val = output->data.f[i];

        if (val > bestscore) { bestscore = val; best = i; }
    }

    String raw = CLASS_LABELS[best];
    Serial.printf("Raw: %s (score %.3f)\n", raw.c_str(), bestscore);

    // Optional: map to category and upload to Firebase
    String category = raw; // or keep your mapping
    firebasePutString("/classification/latest.json", "\"" + category + "\"");

    // Release frame buffer
    esp_camera_fb_return(fb);

    delay(500);
}

/* ---------- Helper functions ---------- */
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
    config.pixel_format = PIXFORMAT_RGB565;  
    config.frame_size = FRAMESIZE_QCIF;   // 128x96 — safe for OV3660
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        while (true) delay(1000);
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) return;

    if (s->id.PID == OV3660_PID) {
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
        s->set_vflip(s, 1);
        // DO NOT call set_framesize here — already set in config
    }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
#endif
}

void firebasePutString(const String &pathJson, const String &jsonValue) {
  HTTPClient http;
  // Build URL; note: include auth token if given
  String url = String(FIREBASE_ROOT) + pathJson;
  if (strlen(FIREBASE_AUTH) > 0) {
    // Firebase RTDB expects ?auth=TOKEN (or append ?auth=token to the URL)
    // Many examples append "?auth=TOKEN" (not path). Adjust accordingly:
    if (pathJson.indexOf('?') == -1) url += String("?auth=") + String(FIREBASE_AUTH);
    else url += String("&auth=") + String(FIREBASE_AUTH);
  }

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(jsonValue);
  String resp = http.getString();
  Serial.printf("FB PUT code=%d resp=%s\n", code, resp.c_str());
  http.end();
}

// Nearest-neighbor resize from RGB565 FB into model input (uint8 RGB)
void preprocess_rgb565_to_uint8(camera_fb_t *fb) {
  // assume input tensor is uint8 and expects [1, H, W, 3] layout in row-major
  uint8_t *out = input_tensor->data.uint8;
  int srcW = fb->width;
  int srcH = fb->height;
  int dstW = IMG_W, dstH = IMG_H;
  uint8_t *src = fb->buf;

  // RGB565 uses 2 bytes per pixel in buffer
  for (int y=0; y<dstH; y++) {
    int sy = (y * srcH) / dstH;
    for (int x=0; x<dstW; x++) {
      int sx = (x * srcW) / dstW;
      int idx = (sy * srcW + sx) * 2;
      if (idx + 1 >= fb->len) {
        out[(y*dstW + x)*3 + 0] = 128;
        out[(y*dstW + x)*3 + 1] = 128;
        out[(y*dstW + x)*3 + 2] = 128;
        continue;
      }

      uint8_t b1 = src[idx];
      uint8_t b2 = src[idx+1];
      uint16_t pix = (uint16_t)b2 << 8 | b1;
      uint8_t r5 = (pix >> 11) & 0x1F;
      uint8_t g6 = (pix >> 5) & 0x3F;
      uint8_t b5 = pix & 0x1F;

      uint8_t r = (r5 * 255) / 31;
      uint8_t g = (g6 * 255) / 63;
      uint8_t b = (b5 * 255) / 31;

      out[(y*dstW + x)*3 + 0] = r;
      out[(y*dstW + x)*3 + 1] = g;
      out[(y*dstW + x)*3 + 2] = b;
    }
  }
}