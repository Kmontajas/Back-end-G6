#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


#define WIFI_SSID "YourWiFiName"
#define WIFI_PASSWORD "YourWiFiPassword"
#define API_KEY "AIzaSyBgR79fzzSvRGC7eDGIpm3R_3AVsrnnKcg"
#define DATABASE_URL "https://smart-trashbin-4c1d0-default-rtdb.asia-southeast1.firebasedatabase.app/"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

#define SERVO_REC 4
#define SERVO_BIO 22
#define SERVO_NON 26

Servo servoRec;
Servo servoBio;
Servo servoNon;

#define TRIG_REC 5
#define ECHO_REC 18
#define TRIG_BIO 19
#define ECHO_BIO 21
#define TRIG_NON 23
#define ECHO_NON 25

int binFullLevel = 10;
bool recNotified=false, bioNotified=false, nonNotified=false;
String lastLabel = ""; 

float getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 30000); 
  if (duration == 0) return -1;
  return duration * 0.034 / 2;
}

void openLid(Servo &s) {
  s.write(90);
  delay(2500);
  s.write(0);
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  servoRec.attach(SERVO_REC);
  servoBio.attach(SERVO_BIO);
  servoNon.attach(SERVO_NON);
  servoRec.write(0); servoBio.write(0); servoNon.write(0);

  pinMode(TRIG_REC, OUTPUT); pinMode(ECHO_REC, INPUT);
  pinMode(TRIG_BIO, OUTPUT); pinMode(ECHO_BIO, INPUT);
  pinMode(TRIG_NON, OUTPUT); pinMode(ECHO_NON, INPUT);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { Serial.print("."); delay(400); }
  Serial.println("\nConnected: " + WiFi.localIP());

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  Serial.println("Firebase ready");
}

void checkAndActOnClassification() {
  if (!Firebase.ready()) return;
  if (Firebase.RTDB.getString(&fbdo, "/classification/latest")) {
    String label = fbdo.stringData();
    if (label.length() > 0 && label != lastLabel) {
      Serial.println("New classification: " + label);
      lastLabel = label;

      if (label.equalsIgnoreCase("recyclable")) {
        openLid(servoRec);
      } else if (label.equalsIgnoreCase("biodegradable")) {
        openLid(servoBio);
      } else if (label.equalsIgnoreCase("nonbiodegradable") || label.equalsIgnoreCase("non-biodegradable")) {
        openLid(servoNon);
      } else {
        Serial.println("Unknown label: " + label);
      }

      Firebase.RTDB.setString(&fbdo, "/classification/latest", "");
    } 
  } else {
    Serial.println("Failed to read classification: " + fbdo.errorReason());
  }
}

void updateFillLevels() {
  float drec = getDistance(TRIG_REC, ECHO_REC);
  if (drec > 0 && Firebase.ready()) {
    Firebase.RTDB.setFloat(&fbdo, "/bins/recyclable/fillLevel", drec);
    if (drec < binFullLevel && !recNotified) {
      Firebase.RTDB.setString(&fbdo, "/bins/recyclable/status", "FULL");
      recNotified = true;
    } else if (drec >= binFullLevel + 5) {
      Firebase.RTDB.setString(&fbdo, "/bins/recyclable/status", "OK");
      recNotified = false;
    }
  }

  float dbio = getDistance(TRIG_BIO, ECHO_BIO);
  if (dbio > 0 && Firebase.ready()) {
    Firebase.RTDB.setFloat(&fbdo, "/bins/biodegradable/fillLevel", dbio);
    if (dbio < binFullLevel && !bioNotified) {
      Firebase.RTDB.setString(&fbdo, "/bins/biodegradable/status", "FULL");
      bioNotified = true;
    } else if (dbio >= binFullLevel + 5) {
      Firebase.RTDB.setString(&fbdo, "/bins/biodegradable/status", "OK");
      bioNotified = false;
    }
  }

  float dnon = getDistance(TRIG_NON, ECHO_NON);
  if (dnon > 0 && Firebase.ready()) {
    Firebase.RTDB.setFloat(&fbdo, "/bins/nonbiodegradable/fillLevel", dnon);
    if (dnon < binFullLevel && !nonNotified) {
      Firebase.RTDB.setString(&fbdo, "/bins/nonbiodegradable/status", "FULL");
      nonNotified = true;
    } else if (dnon >= binFullLevel + 5) {
      Firebase.RTDB.setString(&fbdo, "/bins/nonbiodegradable/status", "OK");
      nonNotified = false;
    }
  }
}

unsigned long lastLoop = 0;
void loop() {
  unsigned long now = millis();
  if (now - lastLoop >= 3000) { 
    lastLoop = now;
    checkAndActOnClassification();
    updateFillLevels();
  }
}
