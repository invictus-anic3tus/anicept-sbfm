/*
 * LM393 Photoelectric Speed Sensor — ESP32-C3 SuperMini
 *
 * Measures linear speed in mm/s.
 * Each full blockage cycle = 1.2565 mm of movement.
 *
 * Wiring:
 *   LM393 D0 (signal) → GPIO 1
 *   LM393 VCC         → 3.3V
 *   LM393 GND         → GND
 */

// ── Pin definitions ──────────────────────────────────────────
#define SENSOR_PIN_A   1   // Primary LM393 signal (D0)

// ── Physical constant ────────────────────────────────────────
const float MM_PER_CYCLE = 1.2565f;  // mm per full on/off cycle

// ── Speed averaging ──────────────────────────────────────────
const int   AVG_WINDOW     = 5;      // rolling-average window (pulses)
const float STALL_TIMEOUT  = 500.0f; // ms — declare zero speed after this

// ── Globals ──────────────────────────────────────────────────
volatile unsigned long lastPulseTime  = 0;
volatile unsigned long pulseIntervalUs = 0;
volatile bool          newPulse       = false;

// Rolling average buffer
float intervalBuf[AVG_WINDOW];
int   bufIdx   = 0;
int   bufCount = 0;

// ── ISR ──────────────────────────────────────────────────────
void IRAM_ATTR onPulseA() {
  unsigned long now = micros();

  if (lastPulseTime != 0) {
    pulseIntervalUs = now - lastPulseTime;
  }
  lastPulseTime = now;
  newPulse = true;
}

// ── Rolling average helper ────────────────────────────────────
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

// ── Setup ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(SENSOR_PIN_A, INPUT_PULLUP);
  pinMode(SENSOR_PIN_B, INPUT_PULLUP);

  // Trigger on RISING edge (LOW→HIGH as disc slot opens)
  attachInterrupt(digitalPinToInterrupt(SENSOR_PIN_A), onPulseA, RISING);

  Serial.println("=== LM393 Speed Sensor — ESP32-C3 ===");
  Serial.println("Speed (mm/s)");
  Serial.println("─────────────────────────────────────");
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
  static unsigned long lastPrint = 0;

  // ── Consume a new pulse ──
  if (newPulse) {
    noInterrupts();
    unsigned long interval = pulseIntervalUs;
    newPulse = false;
    interrupts();

    pushInterval((float)interval);

    float avgUs   = avgInterval();
    float speedMMs = 0.0f;
    if (avgUs > 0) {
      // interval is time for ONE half-cycle (rising edge to rising edge)
      // that equals ONE full cycle distance = MM_PER_CYCLE
      float avgSec = avgUs / 1e6f;
      speedMMs = MM_PER_CYCLE / avgSec;
    }

    Serial.printf("  %8.2f mm/s \n", speedMMs);
  }

  // ── Stall detection (no pulses → speed = 0) ──
  if (millis() - lastPrint >= 200) {
    lastPrint = millis();

    unsigned long nowMs = millis();
    unsigned long lastMs;
    noInterrupts();
    lastMs = lastPulseTime / 1000;  // convert µs → ms approximation
    interrupts();

    bool stalled = (lastMs == 0) || ((nowMs - lastMs) > (unsigned long)STALL_TIMEOUT);
    if (stalled && !newPulse) {
      bufCount = 0;  // clear average so restart is clean
      Serial.println("     0.00 mm/s");
    }
  }
}
