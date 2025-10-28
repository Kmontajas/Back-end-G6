#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"

#define API_KEY "AIzaSyBgR79fzzSvRGC7eDGIpm3R_3AVsrnnKcg"
#define DATABASE_URL "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app/"

const char* classifierEndpoint = "https://your-classifier.example.com/classify"; // replace

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

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
config.pixel_format = PIXFORMAT_JPEG;
config.frame_size = FRAMESIZE_QVGA; // QVGA or lower recommended to keep image small
config.jpeg_quality = 12; // 0-63 lower is better quality (bigger size)
config.fb_count = 1;

esp_err_t err = esp_camera_init(&config);
if (err != ESP_OK) {
Serial.printf("Camera init failed with error 0x%x\n", err);
while (true) delay(1000);
}
}

String postImageAndGetLabel(uint8_t *buf, size_t len) {

HTTPClient http;
String boundary = "----ESP32CAMBOUNDARY";
String contentType = "multipart/form-data; boundary=" + boundary;

http.begin(classifierEndpoint);
http.addHeader("Content-Type", contentType);
http.setTimeout(20000);

String head = "--" + boundary + "\r\n";
head += "Content-Disposition: form-data; name="image"; filename="capture.jpg"\r\n";
head += "Content-Type: image/jpeg\r\n\r\n";

String tail = "\r\n--" + boundary + "--\r\n";

int contentLength = head.length() + len + tail.length();
http.addHeader("Content-Length", String(contentLength));

int ret = http.sendRequest("POST", (uint8_t*)NULL, 0); // start request
WiFiClient * stream = http.getStreamPtr();
stream->print(head);
stream->write(buf, len);
stream->print(tail);

int httpCode = http.GET(); // actually send/finish — some servers require different handling

String label = "";
if (httpCode == HTTP_CODE_OK) {
String payload = http.getString();
Serial.println("Classifier response: " + payload);

int i = payload.indexOf("\"label\"");
if (i >= 0) {
  int colon = payload.indexOf(':', i);
  int q1 = payload.indexOf('"', colon);
  int q2 = payload.indexOf('"', q1 + 1);
  if (q1 >= 0 && q2 > q1) label = payload.substring(q1 + 1, q2);
} else {

  label = payload;
}
} else {
Serial.printf("Classifier HTTP error: %d\n", httpCode);
}
http.end();
return label;
}

void uploadLabelToFirebase(const String &label) {
if (!Firebase.ready()) return;

if (Firebase.RTDB.setString(&fbdo, "/classification/latest", label)) {
Serial.println("Wrote label to Firebase: " + label);
} else {
Serial.println("Failed to write label: " + fbdo.errorReason());
}
}

void setup() {
Serial.begin(115200);
delay(1000);
Serial.println("ESP32-CAM starting...");

WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
Serial.print("Connecting to WiFi");
while (WiFi.status() != WL_CONNECTED) {
Serial.print(".");
delay(500);
}
Serial.println("\nConnected to WiFi: " + WiFi.localIP());

config.api_key = API_KEY;
config.database_url = DATABASE_URL;
Firebase.begin(&config, &auth);
Firebase.reconnectWiFi(true);
Serial.println("Firebase ready");

setupCamera();
Serial.println("Camera initialized");
}

void loop() {
// Wait for a trigger: you can use a proximity sensor, or poll a push button, or run continuously.
// For simplicity here we capture every 8 seconds but in real system you'd trigger when an object is detected.
camera_fb_t * fb = esp_camera_fb_get();
if (!fb) {
Serial.println("Camera capture failed");
delay(2000);
return;
}

Serial.println("Captured image, size: " + String(fb->len));

String label = postImageAndGetLabel(fb->buf, fb->len);

if (label.length() == 0) label = "unknown";

uploadLabelToFirebase(label);

esp_camera_fb_return(fb);
delay(8000); // wait before next capture (adjust as needed)
}
