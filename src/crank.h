#pragma once
#include <Arduino.h>

// Hurdy-gurdy crank sensed by an incremental A/B/Z quadrature encoder.
class Crank {
public:
  void begin();
  // Call every loop iteration; does the maths every ~10 ms.
  void update(uint32_t nowMs);

  float speedRps() const { return _speed; }        // |rev/s|, smoothed
  int   direction() const { return _dir; }         // -1, 0, +1
  float bow() const { return _bow; }               // 0..1 bow pressure
  uint32_t revolutions() const { return _revs; }   // from the Z pulse
  // True once when the crank was suddenly accelerated; strength 0..1
  bool consumeJerk(float &strength);

private:
  uint32_t _lastMs = 0;
  int32_t  _lastCount = 0;
  float _speed = 0, _fast = 0, _slow = 0, _bow = 0;
  int   _dir = 0;
  uint32_t _revs = 0;
  uint32_t _lastJerkMs = 0;
  bool  _jerk = false;
  float _jerkStrength = 0;
};
