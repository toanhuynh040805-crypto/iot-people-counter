#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// =====================================================
// DE TAI 52 - NHOM 17
// DEM KHACH VAO/RA & GIOI HAN SO NGUOI
// 2x HC-SR04 + ESP32 + LCD + LED + Buzzer + MQTT
// =====================================================

#define TRIG_A 5
#define ECHO_A 18
#define TRIG_B 19
#define ECHO_B 23
#define LED_GREEN 25
#define LED_RED 26
#define BUZZER 27
#define SDA_PIN 21
#define SCL_PIN 22

LiquidCrystal_I2C lcd(0x27, 16, 2);

const float DISTANCE_LIMIT_CM = 50.0;
// Cho 30 giay de thao tac slider Wokwi khi demo
const unsigned long PASS_TIMEOUT_MS = 30000;

int totalIn = 0;
int totalOut = 0;
int currentPeople = 0;
int maxPeople = 3;

enum State { IDLE, A_FIRST, B_FIRST };
State state = IDLE;
unsigned long stateStartTime = 0;
bool previousA = false;
bool previousB = false;

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";
const char* MQTT_HOST = "host.wokwi.internal";
const int MQTT_PORT = 1883;

const char* TOPIC_STATUS = "nhom17/people/status";
const char* TOPIC_EVENT = "nhom17/people/event";
const char* TOPIC_CMD_RESET = "nhom17/people/cmd/reset";
const char* TOPIC_CMD_MAX = "nhom17/people/cmd/max";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
unsigned long lastMqttReconnectAttempt = 0;
unsigned long lastSensorLog = 0;

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  if (duration == 0) return 999.0;
  return duration * 0.034 / 2.0;
}

void printPaddedLine(uint8_t row, const String& text) {
  lcd.setCursor(0, row);
  String line = text;
  if (line.length() > 16) line = line.substring(0, 16);
  while (line.length() < 16) line += ' ';
  lcd.print(line);
}

void showIdleScreen() {
  printPaddedLine(0, "IN:" + String(totalIn) + " OUT:" + String(totalOut));

  String line2 = "CUR:" + String(currentPeople) + "/" + String(maxPeople);
  if (currentPeople > maxPeople) line2 += " OVER";
  else if (currentPeople == maxPeople) line2 += " MAX";
  else line2 += " OK";
  printPaddedLine(1, line2);
}

void showWaitForB() {
  printPaddedLine(0, "SENSOR A: ON");
  printPaddedLine(1, "CHO SENSOR B");
}

void showWaitForA() {
  printPaddedLine(0, "SENSOR B: ON");
  printPaddedLine(1, "CHO SENSOR A");
}

void updateOutputs() {
  currentPeople = totalIn - totalOut;
  if (currentPeople < 0) currentPeople = 0;

  // Chi canh bao khi SO NGUOI VUOT QUA gioi han.
  // Vi du maxPeople = 3: 0..3 van LED xanh, tu 4 tro len LED do + buzzer.
  bool overLimit = currentPeople > maxPeople;

  digitalWrite(LED_GREEN, overLimit ? LOW : HIGH);
  digitalWrite(LED_RED, overLimit ? HIGH : LOW);

  if (overLimit) tone(BUZZER, 1000);
  else noTone(BUZZER);

  showIdleScreen();
}

void publishStatus() {
  if (!mqtt.connected()) return;

  String status;
  if (currentPeople > maxPeople) status = "OVER_LIMIT";
  else if (currentPeople == maxPeople) status = "MAX";
  else status = "AVAILABLE";

  String payload = "{\"in\":" + String(totalIn) +
                   ",\"out\":" + String(totalOut) +
                   ",\"current\":" + String(currentPeople) +
                   ",\"max\":" + String(maxPeople) +
                   ",\"status\":\"" + status + "\"}";

  mqtt.publish(TOPIC_STATUS, payload.c_str(), true);
}

void publishEvent(const char* eventName) {
  if (!mqtt.connected()) return;

  String payload = "{\"event\":\"" + String(eventName) +
                   "\",\"in\":" + String(totalIn) +
                   ",\"out\":" + String(totalOut) +
                   ",\"current\":" + String(currentPeople) + "}";

  mqtt.publish(TOPIC_EVENT, payload.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(message);

  if (String(topic) == TOPIC_CMD_RESET &&
      (message == "RESET" || message == "reset" || message == "1")) {
    totalIn = 0;
    totalOut = 0;
    currentPeople = 0;
    state = IDLE;

    updateOutputs();
    publishStatus();
    publishEvent("RESET");
    Serial.println("DA RESET BO DEM");
  }

  if (String(topic) == TOPIC_CMD_MAX) {
    int newMax = message.toInt();
    if (newMax > 0 && newMax <= 999) {
      maxPeople = newMax;
      updateOutputs();
      publishStatus();
      publishEvent("SET_MAX");
      Serial.print("MAX MOI = ");
      Serial.println(maxPeople);
    }
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

  Serial.print("Dang ket noi WiFi");
  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(250);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" OK");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" TIMEOUT - VAN CHAY CAM BIEN");
  }
}

bool connectMqtt() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String clientId = "nhom17-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("Dang ket noi MQTT... ");
  if (!mqtt.connect(clientId.c_str())) {
    Serial.print("FAILED rc=");
    Serial.println(mqtt.state());
    return false;
  }

  mqtt.subscribe(TOPIC_CMD_RESET);
  mqtt.subscribe(TOPIC_CMD_MAX);
  publishStatus();

  Serial.println("OK");
  return true;
}

void confirmIn() {
  totalIn++;
  state = IDLE;
  updateOutputs();
  publishEvent("IN");
  publishStatus();
  Serial.println(">>> KHACH VAO: A -> B");
}

void confirmOut() {
  currentPeople = totalIn - totalOut;

  if (currentPeople > 0) {
    totalOut++;
    Serial.println("<<< KHACH RA: B -> A");
    publishEvent("OUT");
  } else {
    Serial.println("KHONG THE OUT VI CURRENT = 0");
  }

  state = IDLE;
  updateOutputs();
  publishStatus();
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== DE TAI 52 - NHOM 17 ===");

  pinMode(TRIG_A, OUTPUT);
  pinMode(ECHO_A, INPUT);
  pinMode(TRIG_B, OUTPUT);
  pinMode(ECHO_B, INPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  noTone(BUZZER);

  Wire.begin(SDA_PIN, SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  printPaddedLine(0, "DE TAI 52");
  printPaddedLine(1, "NHOM 17");
  delay(800);

  updateOutputs();
  connectWiFi();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setSocketTimeout(1);
  connectMqtt();

  Serial.println("HE THONG SAN SANG");
  Serial.println("< 50cm = PHAT HIEN, >= 50cm = KHONG PHAT HIEN");
}

void loop() {
  // 1) Doc hai cam bien
  float distanceA = readDistance(TRIG_A, ECHO_A);
  delay(60);
  float distanceB = readDistance(TRIG_B, ECHO_B);

  bool detectedA = distanceA < DISTANCE_LIMIT_CM;
  bool detectedB = distanceB < DISTANCE_LIMIT_CM;

  bool eventA = detectedA && !previousA;
  bool eventB = detectedB && !previousB;

  previousA = detectedA;
  previousB = detectedB;

  // Log de test slider Wokwi
  if (millis() - lastSensorLog >= 500) {
    lastSensorLog = millis();
    Serial.print("[SENSOR] A=");
    Serial.print(distanceA, 1);
    Serial.print("cm ");
    Serial.print(detectedA ? "ON" : "OFF");
    Serial.print(" | B=");
    Serial.print(distanceB, 1);
    Serial.print("cm ");
    Serial.print(detectedB ? "ON" : "OFF");
    Serial.print(" | STATE=");
    if (state == IDLE) Serial.println("IDLE");
    else if (state == A_FIRST) Serial.println("A_FIRST");
    else Serial.println("B_FIRST");
  }

  // 2) State machine: A->B = IN, B->A = OUT
  switch (state) {
    case IDLE:
      if (eventA && !eventB) {
        state = A_FIRST;
        stateStartTime = millis();
        showWaitForB();
        Serial.println("A PHAT HIEN TRUOC -> CHO B");
      } else if (eventB && !eventA) {
        state = B_FIRST;
        stateStartTime = millis();
        showWaitForA();
        Serial.println("B PHAT HIEN TRUOC -> CHO A");
      } else if (eventA && eventB) {
        Serial.println("A VA B CUNG PHAT HIEN -> HUY LUOT");
        printPaddedLine(0, "A+B CUNG LUC");
        printPaddedLine(1, "HUY LUOT");
        delay(400);
        updateOutputs();
      }
      break;

    case A_FIRST:
      if (detectedB) {
        confirmIn();
      } else if (millis() - stateStartTime > PASS_TIMEOUT_MS) {
        Serial.println("TIMEOUT A -> HUY LUOT");
        state = IDLE;
        updateOutputs();
      }
      break;

    case B_FIRST:
      if (detectedA) {
        confirmOut();
      } else if (millis() - stateStartTime > PASS_TIMEOUT_MS) {
        Serial.println("TIMEOUT B -> HUY LUOT");
        state = IDLE;
        updateOutputs();
      }
      break;
  }

  // 3) MQTT xu ly sau cung, khong lam anh huong viec dem
  if (mqtt.connected()) {
    mqtt.loop();
  } else if (WiFi.status() == WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 10000) {
      lastMqttReconnectAttempt = now;
      connectMqtt();
    }
  }

  delay(80);
}
