#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// =====================================================
// DE TAI 52 - NHOM 17
// DEM KHACH VAO/RA & GIOI HAN SO NGUOI
// Core: 2x HC-SR04 + ESP32 + OLED + LED + Buzzer
// Network: Wi-Fi + MQTT
// =====================================================

// -------------------------
// PIN MAP
// -------------------------
#define TRIG_A 5
#define ECHO_A 18

#define TRIG_B 19
#define ECHO_B 23

#define LED_GREEN 25
#define LED_RED   26
#define BUZZER    27

#define SDA_PIN 21
#define SCL_PIN 22

// -------------------------
// OLED
// -------------------------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// -------------------------
// SENSOR CONFIG
// -------------------------
const float DISTANCE_LIMIT_CM = 50.0;
const unsigned long PASS_TIMEOUT_MS = 6000;

// -------------------------
// COUNTER
// -------------------------
int totalIn = 0;
int totalOut = 0;
int currentPeople = 0;
int maxPeople = 3;

// -------------------------
// STATE MACHINE
// -------------------------
enum State {
  IDLE,
  A_FIRST,
  B_FIRST
};

State state = IDLE;
unsigned long stateStartTime = 0;

bool previousA = false;
bool previousB = false;

// -------------------------
// WIFI + MQTT
// -------------------------
// Wokwi Wi-Fi
const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

// Wokwi for VS Code + Private IoT Gateway:
const char* MQTT_HOST = "host.wokwi.internal";
const int MQTT_PORT = 1883;

const char* TOPIC_STATUS = "nhom17/people/status";
const char* TOPIC_EVENT = "nhom17/people/event";
const char* TOPIC_CMD_RESET = "nhom17/people/cmd/reset";
const char* TOPIC_CMD_MAX = "nhom17/people/cmd/max";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

unsigned long lastMqttReconnectAttempt = 0;

// =====================================================
// SENSOR
// =====================================================

float readDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return 999.0;
  }

  return duration * 0.034 / 2.0;
}

// =====================================================
// OUTPUT
// =====================================================

void updateOutputs() {
  currentPeople = totalIn - totalOut;

  if (currentPeople < 0) {
    currentPeople = 0;
  }

  bool full = currentPeople >= maxPeople;

  if (full) {
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, HIGH);
    tone(BUZZER, 1000);
  } else {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, LOW);
    noTone(BUZZER);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, 0);
  display.println("DE TAI 52 - NHOM 17");

  display.setCursor(0, 12);
  display.print("IN      : ");
  display.println(totalIn);

  display.print("OUT     : ");
  display.println(totalOut);

  display.print("CURRENT : ");
  display.println(currentPeople);

  display.print("MAX     : ");
  display.println(maxPeople);

  display.setCursor(0, 55);

  if (full) {
    display.print("*** DA DAY ***");
  } else {
    display.print("CON TRONG: ");
    display.print(maxPeople - currentPeople);
  }

  display.display();
}

// =====================================================
// MQTT
// =====================================================

void publishStatus() {
  if (!mqtt.connected()) return;

  String status = (currentPeople >= maxPeople) ? "FULL" : "AVAILABLE";

  String payload = "{";
  payload += "\"in\":" + String(totalIn) + ",";
  payload += "\"out\":" + String(totalOut) + ",";
  payload += "\"current\":" + String(currentPeople) + ",";
  payload += "\"max\":" + String(maxPeople) + ",";
  payload += "\"status\":\"" + status + "\"";
  payload += "}";

  mqtt.publish(TOPIC_STATUS, payload.c_str(), true);
}

void publishEvent(const char* eventName) {
  if (!mqtt.connected()) return;

  String payload = "{";
  payload += "\"event\":\"" + String(eventName) + "\",";
  payload += "\"in\":" + String(totalIn) + ",";
  payload += "\"out\":" + String(totalOut) + ",";
  payload += "\"current\":" + String(currentPeople);
  payload += "}";

  mqtt.publish(TOPIC_EVENT, payload.c_str(), false);
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

  if (String(topic) == TOPIC_CMD_RESET) {
    if (message == "1" || message == "RESET" || message == "reset") {
      totalIn = 0;
      totalOut = 0;
      currentPeople = 0;

      updateOutputs();
      publishStatus();
      publishEvent("RESET");

      Serial.println("DA RESET BO DEM");
    }
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
  Serial.print("Dang ket noi Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Wi-Fi OK. IP: ");
  Serial.println(WiFi.localIP());
}

bool connectMqtt() {
  Serial.print("Dang ket noi MQTT... ");

  String clientId = "nhom17-esp32-";
  clientId += String((uint32_t)ESP.getEfuseMac(), HEX);

  if (mqtt.connect(clientId.c_str())) {
    Serial.println("OK");

    mqtt.subscribe(TOPIC_CMD_RESET);
    mqtt.subscribe(TOPIC_CMD_MAX);

    publishStatus();
    return true;
  }

  Serial.print("FAILED, rc=");
  Serial.println(mqtt.state());
  return false;
}

// =====================================================
// COUNTER
// =====================================================

void confirmIn() {
  totalIn++;
  updateOutputs();

  Serial.println(">>> KHACH VAO: A -> B");
  publishEvent("IN");
  publishStatus();

  state = IDLE;
}

void confirmOut() {
  currentPeople = totalIn - totalOut;

  if (currentPeople > 0) {
    totalOut++;
    updateOutputs();

    Serial.println("<<< KHACH RA: B -> A");
    publishEvent("OUT");
    publishStatus();
  } else {
    Serial.println("KHONG THE OUT VI CURRENT = 0");
  }

  state = IDLE;
}

// =====================================================
// SETUP
// =====================================================

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

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("LOI KHOI DONG OLED!");

    while (true) {
      delay(100);
    }
  }

  updateOutputs();

  connectWiFi();

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);

  connectMqtt();

  Serial.println("HE THONG SAN SANG");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // MQTT reconnect
  if (!mqtt.connected()) {
    unsigned long now = millis();

    if (now - lastMqttReconnectAttempt > 3000) {
      lastMqttReconnectAttempt = now;
      connectMqtt();
    }
  } else {
    mqtt.loop();
  }

  // Doc A
  float distanceA = readDistance(TRIG_A, ECHO_A);

  // Giam cross-talk giua hai HC-SR04
  delay(60);

  // Doc B
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
      if (eventB) {
        confirmIn();
      } else if (millis() - stateStartTime > PASS_TIMEOUT_MS) {
        Serial.println("TIMEOUT A -> HUY LUOT");
        state = IDLE;
      }
      break;

    case B_FIRST:
      if (eventA) {
        confirmOut();
      } else if (millis() - stateStartTime > PASS_TIMEOUT_MS) {
        Serial.println("TIMEOUT B -> HUY LUOT");
        state = IDLE;
      }
      break;
  }

  delay(80);
}
