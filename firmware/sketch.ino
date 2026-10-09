#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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
const unsigned long PASS_TIMEOUT_MS = 6000;

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

void updateOutputs() {
  currentPeople = totalIn - totalOut;
  if (currentPeople < 0) currentPeople = 0;

  bool full = currentPeople >= maxPeople;

  digitalWrite(LED_GREEN, full ? LOW : HIGH);
  digitalWrite(LED_RED, full ? HIGH : LOW);

  if (full) tone(BUZZER, 1000);
  else noTone(BUZZER);

  printPaddedLine(0, "IN:" + String(totalIn) + " OUT:" + String(totalOut));

  String line2 = "CUR:" + String(currentPeople) + "/" + String(maxPeople);
  line2 += full ? " FULL" : " OK";
  printPaddedLine(1, line2);
}

void publishStatus() {
  if (!mqtt.connected()) return;

  String status = currentPeople >= maxPeople ? "FULL" : "AVAILABLE";
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
  for (unsigned int i = 0; i < length; i++) message += (char)payload[i];

  if (String(topic) == TOPIC_CMD_RESET &&
      (message == "RESET" || message == "reset" || message == "1")) {
    totalIn = 0;
    totalOut = 0;
    currentPeople = 0;
    state = IDLE;
    updateOutputs();
    publishStatus();
    publishEvent("RESET");
  }

  if (String(topic) == TOPIC_CMD_MAX) {
    int newMax = message.toInt();
    if (newMax > 0 && newMax <= 999) {
      maxPeople = newMax;
      updateOutputs();
      publishStatus();
      publishEvent("SET_MAX");
    }
  }
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);
  Serial.print("Dang ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print('.');
  }
  Serial.println(" OK");
}

bool connectMqtt() {
  String clientId = "nhom17-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  if (!mqtt.connect(clientId.c_str())) {
    Serial.print("MQTT failed rc=");
    Serial.println(mqtt.state());
    return false;
  }

  mqtt.subscribe(TOPIC_CMD_RESET);
  mqtt.subscribe(TOPIC_CMD_MAX);
  publishStatus();
  Serial.println("MQTT connected");
  return true;
}

void confirmIn() {
  totalIn++;
  updateOutputs();
  publishEvent("IN");
  publishStatus();
  Serial.println(">>> KHACH VAO: A -> B");
  state = IDLE;
}

void confirmOut() {
  currentPeople = totalIn - totalOut;

  if (currentPeople > 0) {
    totalOut++;
    updateOutputs();
    publishEvent("OUT");
    publishStatus();
    Serial.println("<<< KHACH RA: B -> A");
  }

  state = IDLE;
}

void setup() {
  Serial.begin(115200);

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

  updateOutputs();
  connectWiFi();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  connectMqtt();

  Serial.println("HE THONG SAN SANG");
}

void loop() {
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 3000) {
      lastMqttReconnectAttempt = now;
      connectMqtt();
    }
  } else {
    mqtt.loop();
  }

  float distanceA = readDistance(TRIG_A, ECHO_A);
  delay(60);
  float distanceB = readDistance(TRIG_B, ECHO_B);

  bool detectedA = distanceA < DISTANCE_LIMIT_CM;
  bool detectedB = distanceB < DISTANCE_LIMIT_CM;
  bool eventA = detectedA && !previousA;
  bool eventB = detectedB && !previousB;

  previousA = detectedA;
  previousB = detectedB;

  switch (state) {
    case IDLE:
      if (eventA) {
        state = A_FIRST;
        stateStartTime = millis();
        Serial.println("A PHAT HIEN TRUOC -> CHO B");
      } else if (eventB) {
        state = B_FIRST;
        stateStartTime = millis();
        Serial.println("B PHAT HIEN TRUOC -> CHO A");
      }
      break;

    case A_FIRST:
      if (eventB) confirmIn();
      else if (millis() - stateStartTime > PASS_TIMEOUT_MS) {
        Serial.println("TIMEOUT A -> HUY LUOT");
        state = IDLE;
      }
      break;

    case B_FIRST:
      if (eventA) confirmOut();
      else if (millis() - stateStartTime > PASS_TIMEOUT_MS) {
        Serial.println("TIMEOUT B -> HUY LUOT");
        state = IDLE;
      }
      break;
  }

  delay(80);
}
