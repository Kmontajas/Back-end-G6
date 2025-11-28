#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// ===== CONFIG =====
#define WIFI_SSID "Joasher"
#define WIFI_PASSWORD "01051014"
const char *FIREBASE_ROOT = "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app";
const char *FIREBASE_AUTH = "KqVUe0QRhh9dbCBhRhBfCuqDfLVE5suR0xXWO0yF"; 

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
const int FULL_DISTANCE_CM = 10;        // distance considered full
const unsigned long POLL_INTERVAL_MS = 2000;  // poll Firebase every 2s
const unsigned long FULL_ALERT_DELAY = 5000;  // full for 5s before sending alert

// ===== Globals =====
Servo servoRec, servoBio, servoNon;
String lastLabel = "";
unsigned long lastPoll = 0;

// Full alert timers and flags
unsigned long recFullStart = 0, bioFullStart = 0, nonFullStart = 0;
bool recAlertSent = false, bioAlertSent = false, nonAlertSent = false;

// ===== Helpers =====
float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000);  // timeout 30ms
  if (duration == 0) return -1.0;
  return duration * 0.034 / 2;
}

String firebaseGet(const String &pathJson) {
  HTTPClient http;
  String url = String(FIREBASE_ROOT) + pathJson;
  if (strlen(FIREBASE_AUTH) > 0) url += "?auth=" + String(FIREBASE_AUTH);
  http.begin(url);
  int code = http.GET();
  String payload = "";
  if (code == HTTP_CODE_OK) {
    payload = http.getString();
    Serial.println("Firebase GET success: " + payload);
  } else {
    Serial.printf("Firebase GET failed code=%d\n", code);
  }
  http.end();
  return payload;
}

bool firebasePut(const String &pathJson, const String &jsonValue) {
  HTTPClient http;
  String url = String(FIREBASE_ROOT) + pathJson;
  if (strlen(FIREBASE_AUTH) > 0) url += "?auth=" + String(FIREBASE_AUTH);
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(jsonValue);
  String payload = http.getString();
  http.end();
  if (code >= 200 && code < 300) {
    Serial.println("Firebase PUT success: " + jsonValue);
    return true;
  }
  Serial.printf("Firebase PUT failed code=%d payload=%s\n", code, payload.c_str());
  return false;
}

void openLid(Servo &s) {
  s.write(90);
  delay(2500);
  s.write(0);
}

// Check if a bin is full independently of the servo
void checkFull(float dist, unsigned long &startTime, bool &alertSent, const String &binName) {
  if (dist > 0 && dist <= FULL_DISTANCE_CM) {
    if (!alertSent) {
      if (startTime == 0) startTime = millis();  // start timing
      else if (millis() - startTime >= FULL_ALERT_DELAY) {
        firebasePut("/alerts/" + binName + ".json", "{\"time\": " + String(millis()) + "}");
        firebasePut("/bins/" + binName + "/status.json", "\"FULL\"");
        alertSent = true;
        Serial.println(binName + " is FULL! Alert sent.");
      }
    }
  } else {
    startTime = 0;
    if (alertSent) {
      alertSent = false;
      firebasePut("/bins/" + binName + "/status.json", "\"OK\"");
      Serial.println(binName + " is OK now.");
    }
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
  Serial.println("Ready to poll Firebase!");
}

void loop() {
  unsigned long now = millis();
  if (now - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = now;

    // 1) Poll camera classification
    String classific = firebaseGet("/classification/latest.json");
    if (classific.length() > 0) {
      classific.trim();
      if (classific.startsWith("\"") && classific.endsWith("\"") && classific.length() >= 2)
        classific = classific.substring(1, classific.length() - 1);

      if (classific.length() > 0 && classific != lastLabel) {
        Serial.println("New classification: " + classific);
        lastLabel = classific;

        if (classific.equalsIgnoreCase("recyclable")) openLid(servoRec);
        else if (classific.equalsIgnoreCase("biodegradable")) openLid(servoBio);
        else if (classific.equalsIgnoreCase("non-biodegradable") || classific.equalsIgnoreCase("nonbiodegradable")) openLid(servoNon);
        else Serial.println("Unknown classification: " + classific);

        firebasePut("/classification/latest.json", "\"\"");
      }
    }

    // 2) Read ultrasonic sensors
    float dRec = readDistance(TRIG_REC, ECHO_REC);
    float dBio = readDistance(TRIG_BIO, ECHO_BIO);
    float dNon = readDistance(TRIG_NON, ECHO_NON);

    // Update fill levels
    if (dRec > 0) firebasePut("/bins/recyclable/fill_level.json", String(dRec, 2));
    if (dBio > 0) firebasePut("/bins/biodegradable/fill_level.json", String(dBio, 2));
    if (dNon > 0) firebasePut("/bins/nonbiodegradable/fill_level.json", String(dNon, 2));

    // 3) Check FULL alerts (independent of servo)
    checkFull(dRec, recFullStart, recAlertSent, "recyclable");
    checkFull(dBio, bioFullStart, bioAlertSent, "biodegradable");
    checkFull(dNon, nonFullStart, nonAlertSent, "nonbiodegradable");

    // Debug
    Serial.printf("Distances: Rec=%.2f Bio=%.2f Non=%.2f\n", dRec, dBio, dNon);
  }

  delay(20);
}
