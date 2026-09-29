#pragma once
#include <Arduino.h>
#include "config.h"

// Debounced keybox. Highest pressed key wins (like the tangent that shortens
// the string the most on a real hurdy-gurdy).
class Keys {
public:
  void begin();
  void update(uint32_t nowMs);
  // Semitone offset above the open string of the winning key, or -1 if none
  int semitone() const { return _top >= 0 ? KEY_SEMITONES[_top] : -1; }
  int topKey() const { return _top; }
  bool pressed(int i) const { return _state[i]; }

private:
  bool _state[NUM_KEYS] = {};
  bool _raw[NUM_KEYS] = {};
  uint32_t _since[NUM_KEYS] = {};
  int _top = -1;
};

// Mode button with short/long press detection
class Button {
public:
  enum Event { NONE, SHORT_PRESS, LONG_PRESS };
  void begin(int pin);
  Event update(uint32_t nowMs);

private:
  int _pin = -1;
  bool _down = false, _fired = false;
  uint32_t _t0 = 0;
};
