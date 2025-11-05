#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP32Servo.h>

// CONFIG
#define WIFI_SSID "*********"
#define WIFI_PASSWORD "*********"
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

// Settings
const int FULL_DISTANCE_CM = 10;    // measured distance considered full (measured from top)
const int EMPTY_DISTANCE_CM = 50;   // far distance considered empty
const unsigned long POLL_INTERVAL_MS = 2000; // poll Firebase every 2s

// Globals
Servo servoRec, servoBio, servoNon;
String lastLabel = "";
unsigned long lastPoll = 0;

// Helpers
float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); // timeout 30ms
  if (duration == 0) return -1.0;
  float cm = duration * 0.034 / 2;
  return cm;
}

// Simple GET from Firebase path ending with .json
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

void openLid(Servo &s) {
  s.write(90);
  delay(2500);
  s.write(0);
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

    // 1) Poll classification
    String classific = firebaseGet("/classification/latest.json");
    if (classific.length() > 0) {

      classific.trim();
      if (classific.startsWith("\"") && classific.endsWith("\"") && classific.length() >= 2) {
        classific = classific.substring(1, classific.length()-1);
      }
      if (classific.length() > 0 && classific != lastLabel) {
        Serial.println("New classification: " + classific);
        lastLabel = classific;

        if (classific.equalsIgnoreCase("recyclable")) {
          openLid(servoRec);
        } else if (classific.equalsIgnoreCase("biodegradable")) {
          openLid(servoBio);
        } else if (classific.equalsIgnoreCase("non-biodegradable") || classific.equalsIgnoreCase("nonbiodegradable")) {
          openLid(servoNon);
        } else {
          Serial.println("Unknown classification: " + classific);
        }

        
        firebasePut("/classification/latest.json", "\"\""); 
      }
    }

    float dRec = readDistance(TRIG_REC, ECHO_REC);
    float dBio = readDistance(TRIG_BIO, ECHO_BIO);
    float dNon = readDistance(TRIG_NON, ECHO_NON);

    if (dRec > 0) {
      firebasePut("/bins/recyclable/fillLevel.json", String(dRec, 2));
      if (dRec < FULL_DISTANCE_CM) {
        firebasePut("/bins/recyclable/status.json", "\"FULL\"");
        firebasePut("/alerts/recyclable.json", "{\"time\": " + String(millis()) + "}");
      } else {
        firebasePut("/bins/recyclable/status.json", "\"OK\"");
      }
    }

    if (dBio > 0) {
      firebasePut("/bins/biodegradable/fillLevel.json", String(dBio, 2));
      if (dBio < FULL_DISTANCE_CM) {
        firebasePut("/bins/biodegradable/status.json", "\"FULL\"");
        firebasePut("/alerts/biodegradable.json", "{\"time\": " + String(millis()) + "}");
      } else {
        firebasePut("/bins/biodegradable/status.json", "\"OK\"");
      }
    }

    if (dNon > 0) {
      firebasePut("/bins/nonbiodegradable/fillLevel.json", String(dNon, 2));
      if (dNon < FULL_DISTANCE_CM) {
        firebasePut("/bins/nonbiodegradable/status.json", "\"FULL\"");
        firebasePut("/alerts/nonbiodegradable.json", "{\"time\": " + String(millis()) + "}");
      } else {
        firebasePut("/bins/nonbiodegradable/status.json", "\"OK\"");
      }
    }
  } // poll interval

  // small yield
  delay(20);
}
