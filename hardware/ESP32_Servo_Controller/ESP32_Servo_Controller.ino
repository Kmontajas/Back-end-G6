#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// ===== CONFIG =====
#define WIFI_SSID "Joasher"
#define WIFI_PASSWORD "01051014"
const char *FIREBASE_ROOT = "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app";
const char *FIREBASE_AUTH = "";

// Servos
#define SERVO_REC_PIN 18
#define SERVO_BIO_PIN 19
#define SERVO_NON_PIN 21

// Ultrasonic sensors (TRIG/ECHO pairs)
#define TRIG_REC 5
#define ECHO_REC 17

#define TRIG_BIO 16
#define ECHO_BIO 4

#define TRIG_NON 15
#define ECHO_NON 2

// ===== Settings =====
const int FULL_DISTANCE_CM = 2;
const unsigned long POLL_INTERVAL_MS = 300; // poll Firebase every 0.3s
const unsigned long LID_OPEN_MS = 3000;     // lid stays open 3 seconds

// ===== Globals =====
Servo servoRec, servoBio, servoNon;
unsigned long lastPoll = 0;

// ===== Bin Structure =====
struct Bin {
  Servo* servo;
  bool isOpen;
  unsigned long lidTimer;
};

Bin recBin = {&servoRec, false, 0};
Bin bioBin = {&servoBio, false, 0};
Bin nonBin = {&servoNon, false, 0};

// ===== Helpers =====
float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  if (duration == 0) return -1.0;
  return duration * 0.034 / 2;
}

// Firebase GET
String firebaseGet(const String &pathJson) {
  HTTPClient http;
  String url = String(FIREBASE_ROOT) + pathJson;
  if (strlen(FIREBASE_AUTH) > 0) url += String(FIREBASE_AUTH);
  http.begin(url);
  int code = http.GET();
  String payload = "";
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
  } else {
    Serial.printf("Firebase GET failed code=%d\n", code);
  }
  http.end();
  return payload;
}

// Firebase PUT
bool firebasePut(const String &pathJson, const String &jsonValue) {
  HTTPClient http;
  String url = String(FIREBASE_ROOT) + pathJson;
  if (strlen(FIREBASE_AUTH) > 0) url += String(FIREBASE_AUTH);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(jsonValue);
  String payload = http.getString();
  http.end();
  if (code >= 200 && code < 300) return true;
  Serial.printf("Firebase PUT failed code=%d payload=%s\n", code, payload.c_str());
  return false;
}

// Open bin lid (non-blocking)
void openBin(Bin &bin) {
  bin.servo->write(90);             // open lid
  bin.isOpen = true;
  bin.lidTimer = millis() + LID_OPEN_MS; // schedule close
}

// Update bin state
void updateBin(Bin &bin) {
  if (bin.isOpen && millis() > bin.lidTimer) {
    bin.servo->write(0); // close lid
    bin.isOpen = false;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Attach servos
  servoRec.attach(SERVO_REC_PIN);
  servoBio.attach(SERVO_BIO_PIN);
  servoNon.attach(SERVO_NON_PIN);
  servoRec.write(0); servoBio.write(0); servoNon.write(0);

  // Setup ultrasonic pins
  pinMode(TRIG_REC, OUTPUT); pinMode(ECHO_REC, INPUT);
  pinMode(TRIG_BIO, OUTPUT); pinMode(ECHO_BIO, INPUT);
  pinMode(TRIG_NON, OUTPUT); pinMode(ECHO_NON, INPUT);

  // WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(300); }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
}

void loop() {
  unsigned long now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;

    // Poll classification
    String classific = firebaseGet("/classification/latest.json");
    if (classific.length() > 0) {
      classific.trim();
      if (classific.startsWith("\"") && classific.endsWith("\"") && classific.length() >= 2)
        classific = classific.substring(1, classific.length()-1);

      if (classific.length() > 0) {
        Serial.println("New classification: " + classific);

        if (classific.equalsIgnoreCase("recyclable")) openBin(recBin);
        else if (classific.equalsIgnoreCase("biodegradable")) openBin(bioBin);
        else if (classific.equalsIgnoreCase("non_biodegradable")) openBin(nonBin);

        else Serial.println("Unknown classification: " + classific);

        // Clear Firebase classification to allow repeated triggers
        firebasePut("/classification/latest.json", "\"\"");
      }
    }

// Read distances
float dRec = readDistance(TRIG_REC, ECHO_REC);
float dBio = readDistance(TRIG_BIO, ECHO_BIO);
float dNon = readDistance(TRIG_NON, ECHO_NON);

// Helper to convert distance to % full
auto distanceToPercent = [](float distance, float emptyDist) -> float {
  float percent = (1.0 - (distance / emptyDist)) * 100.0;
  return constrain(percent, 0, 100);
};

const float EMPTY_DISTANCE_CM = 15.0; // max distance = empty bin

// --- Recyclable ---
if (dRec > 0) {
  float fillPercent = distanceToPercent(dRec, EMPTY_DISTANCE_CM);
  firebasePut("/bins/recyclable/fill_level.json", String(dRec, 2));           // raw distance
  firebasePut("/bins/recyclable/fill_level_percent.json", String(fillPercent, 1)); // % full
  firebasePut("/bins/recyclable/status.json", dRec < FULL_DISTANCE_CM ? "\"FULL\"" : "\"OK\"");
  if (dRec < FULL_DISTANCE_CM) firebasePut("/alerts/recyclable.json", "{\"time\": " + String(millis()) + "}");
}

// --- Biodegradable ---
if (dBio > 0) {
  float fillPercent = distanceToPercent(dBio, EMPTY_DISTANCE_CM);
  firebasePut("/bins/biodegradable/fill_level.json", String(dBio, 2));
  firebasePut("/bins/biodegradable/fill_level_percent.json", String(fillPercent, 1));
  firebasePut("/bins/biodegradable/status.json", dBio < FULL_DISTANCE_CM ? "\"FULL\"" : "\"OK\"");
  if (dBio < FULL_DISTANCE_CM) firebasePut("/alerts/biodegradable.json", "{\"time\": " + String(millis()) + "}");
}

// --- Non-biodegradable ---
if (dNon > 0) {
  float fillPercent = distanceToPercent(dNon, EMPTY_DISTANCE_CM);
  firebasePut("/bins/non_biodegradable/fill_level.json", String(dNon, 2));
  firebasePut("/bins/non_biodegradable/fill_level_percent.json", String(fillPercent, 1));
  firebasePut("/bins/non_biodegradable/status.json", dNon < FULL_DISTANCE_CM ? "\"FULL\"" : "\"OK\"");
  if (dNon < FULL_DISTANCE_CM) firebasePut("/alerts/non_biodegradable.json", "{\"time\": " + String(millis()) + "}");
}

  }

  // Update lid states
  updateBin(recBin);
  updateBin(bioBin);
  updateBin(nonBin);

  // Small yield
  delay(20);
}
