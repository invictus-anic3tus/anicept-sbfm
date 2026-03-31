/*
 * LM393 Photoelectric Speed Sensor — ESP32-C3 SuperMini
 * Publishes speed data over MQTT to a Raspberry Pi.
 *
 * Wiring:
 *   LM393 D0 (signal) → GPIO 1
 *   LM393 VCC         → 3.3V
 *   LM393 GND         → GND
 *
 * Dependencies (install via Arduino Library Manager):
 *   - PubSubClient  by Nick O'Leary
 *   - ArduinoJson   by Benoit Blanchon
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

// ── User config ───────────────────────────────────────────────
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"
#define MQTT_BROKER     "192.168.1.XXX"   // IP of your Raspberry Pi
#define MQTT_PORT       1883
#define MQTT_TOPIC      "printer/filament/speed"
#define MQTT_CLIENT_ID  "esp32_filament_sensor"

// Optional MQTT auth — leave blank if not used
#define MQTT_USER       ""
#define MQTT_PASS       ""

// ── Pin & physical constant ───────────────────────────────────
#define SENSOR_PIN      1
const float MM_PER_CYCLE = 1.2565f;

// ── Timing ────────────────────────────────────────────────────
const int   AVG_WINDOW      = 5;      // rolling-average window (pulses)
const float STALL_TIMEOUT   = 500.0f; // ms — report zero speed after this
const int   PUBLISH_INTERVAL = 100;  // ms between MQTT publishes

// ── Globals ───────────────────────────────────────────────────
volatile unsigned long lastPulseUs   = 0;
volatile unsigned long intervalUs    = 0;
volatile bool          newPulse      = false;

float  intervalBuf[AVG_WINDOW];
int    bufIdx   = 0;
int    bufCount = 0;

WiFiClient   wifiClient;
PubSubClient mqtt(wifiClient);

// ── ISR ───────────────────────────────────────────────────────
void IRAM_ATTR onPulse() {
  unsigned long now = micros();
  if (lastPulseUs != 0) {
    intervalUs = now - lastPulseUs;
  }
  lastPulseUs = now;
  newPulse = true;
}

// ── Rolling average ───────────────────────────────────────────
void pushInterval(float us) {
  intervalBuf[bufIdx] = us;
  bufIdx = (bufIdx + 1) % AVG_WINDOW;
  if (bufCount < AVG_WINDOW) bufCount++;
}

float avgInterval() {
  if (bufCount == 0) return 0.0f;
  float sum = 0;
  for (int i = 0; i < bufCount; i++) sum += intervalBuf[i];
  return sum / bufCount;
}

// ── WiFi ──────────────────────────────────────────────────────
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\nWiFi connected — IP: %s\n", WiFi.localIP().toString().c_str());
}

// ── MQTT ──────────────────────────────────────────────────────
void connectMQTT() {
  while (!mqtt.connected()) {
    Serial.print("Connecting to MQTT broker...");
    bool ok = (strlen(MQTT_USER) > 0)
      ? mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)
      : mqtt.connect(MQTT_CLIENT_ID);

    if (ok) {
      Serial.println(" connected.");
    } else {
      Serial.printf(" failed (rc=%d), retrying in 3s\n", mqtt.state());
      delay(3000);
    }
  }
}

// ── Publish ───────────────────────────────────────────────────
void publishSpeed(float speedMMs, bool stalled) {
  StaticJsonDocument<128> doc;
  doc["speed_mm_s"] = serialized(String(speedMMs, 3));
  doc["stalled"]    = stalled;
  doc["ts_ms"]      = millis();

  char buf[128];
  serializeJson(doc, buf);
  mqtt.publish(MQTT_TOPIC, buf);
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);

  pinMode(SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), onPulse, RISING);

  connectWiFi();
  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  connectMQTT();

  Serial.println("=== LM393 MQTT publisher running ===");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();

  // Consume new pulse
  if (newPulse) {
    noInterrupts();
    unsigned long iv = intervalUs;
    newPulse = false;
    interrupts();
    pushInterval((float)iv);
  }

  // Publish on interval
  static unsigned long lastPublish = 0;
  if (millis() - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = millis();

    // Stall detection
    unsigned long lastMs;
    noInterrupts();
    lastMs = lastPulseUs / 1000;
    interrupts();

    bool stalled = (lastMs == 0) || ((millis() - lastMs) > (unsigned long)STALL_TIMEOUT);
    float speedMMs = 0.0f;

    if (!stalled && bufCount > 0) {
      float avg = avgInterval();
      if (avg > 0) {
        speedMMs = MM_PER_CYCLE / (avg / 1e6f);
      }
    }

    if (stalled) {
      bufCount = 0;
      speedMMs = 0.0f;
    }

    publishSpeed(speedMMs, stalled);

    Serial.printf("Published: %.3f mm/s%s\n",
      speedMMs, stalled ? " [STOPPED]" : "");
  }
}
