#include "crank.h"
#include "config.h"

namespace {
volatile int32_t g_count = 0;
volatile uint32_t g_index = 0;
volatile uint8_t g_prev = 0;

// Gray-code transition table: index = (prev << 2) | current
const int8_t QDEC[16] = {0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0};

void IRAM_ATTR isrAB() {
  uint8_t cur = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  g_count += QDEC[(g_prev << 2) | cur];
  g_prev = cur;
}
void IRAM_ATTR isrZ() { g_index = g_index + 1; }
}  // namespace

void Crank::begin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_Z, INPUT_PULLUP);
  g_prev = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), isrAB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), isrAB, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_Z), isrZ, FALLING);
  _lastMs = millis();
  _lastCount = g_count;
}

void Crank::update(uint32_t nowMs) {
  uint32_t dt = nowMs - _lastMs;
  if (dt < 10) return;
  int32_t c = g_count;
  int32_t d = c - _lastCount;
  _lastCount = c;
  _lastMs = nowMs;
  _revs = g_index;

  float inst = fabsf((float)d) / ENC_COUNTS_PER_REV / (dt / 1000.0f);
  if (d != 0) _dir = d > 0 ? 1 : -1;
  else if (inst == 0 && _speed < 0.02f) _dir = 0;

  _fast += (inst - _fast) * 0.5f;
  _slow += (inst - _slow) * 0.06f;
  _speed = _fast;

  // Bow pressure: silent under the threshold, then a gentle curve to full
  float x = (_speed - CRANK_MIN_RPS) / (CRANK_FULL_RPS - CRANK_MIN_RPS);
  x = constrain(x, 0.0f, 1.0f);
  _bow = sqrtf(x);

  // Buzzing-bridge trigger: fast average clearly above slow average
  float diff = _fast - _slow;
  if (diff > JERK_THRESHOLD_RPS && _fast > CRANK_MIN_RPS &&
      nowMs - _lastJerkMs > JERK_COOLDOWN_MS) {
    _jerk = true;
    _jerkStrength = constrain(diff / 2.0f + 0.35f, 0.0f, 1.0f);
    _lastJerkMs = nowMs;
  }
}

bool Crank::consumeJerk(float &strength) {
  if (!_jerk) return false;
  _jerk = false;
  strength = _jerkStrength;
  return true;
}
