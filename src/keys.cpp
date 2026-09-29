#include "keys.h"

void Keys::begin() {
  for (int i = 0; i < NUM_KEYS; i++) pinMode(KEY_PINS[i], INPUT_PULLUP);
}

void Keys::update(uint32_t nowMs) {
  for (int i = 0; i < NUM_KEYS; i++) {
    bool r = digitalRead(KEY_PINS[i]) == LOW;
    if (r != _raw[i]) { _raw[i] = r; _since[i] = nowMs; }
    else if (r != _state[i] && nowMs - _since[i] >= KEY_DEBOUNCE_MS) _state[i] = r;
  }
  int top = -1, best = -1;
  for (int i = 0; i < NUM_KEYS; i++)
    if (_state[i] && KEY_SEMITONES[i] > best) { best = KEY_SEMITONES[i]; top = i; }
  _top = top;
}

void Button::begin(int pin) {
  _pin = pin;
  pinMode(pin, INPUT_PULLUP);
}

Button::Event Button::update(uint32_t nowMs) {
  bool down = digitalRead(_pin) == LOW;
  if (down && !_down) { _down = true; _fired = false; _t0 = nowMs; }
  else if (down && _down && !_fired && nowMs - _t0 >= LONG_PRESS_MS) {
    _fired = true;
    return LONG_PRESS;
  } else if (!down && _down) {
    _down = false;
    if (!_fired && nowMs - _t0 >= 25) return SHORT_PRESS;
  }
  return NONE;
}
