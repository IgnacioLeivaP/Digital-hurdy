#pragma once
#include <Arduino.h>

// Hurdy-gurdy voice model:
//   * melody string  : two slightly detuned band-limited saws (unison strings),
//                      pitch chosen by keys; the open string sounds with no key
//   * bourdon+mouche : two drone strings (tonic and fifth)
//   * trompette      : buzzing bridge, kicked by sudden crank acceleration
//   * wheel          : rosin noise proportional to bow pressure
// Every string is excited by the wheel: bow pressure (0..1) sets loudness and
// brightness. All setters are safe to call from another task.
class HurdySynth {
public:
  void begin();

  void setBow(float bow01)          { _bow = bow01; }
  void setMelodyNote(float midi)    { _melNote = midi; }        // fractional MIDI note
  void setMelodyLevel(float lvl)    { _melLevel = lvl; }        // 0..1 (velocity)
  void setDroneRoot(int bourdonMidi){ _bourdon = bourdonMidi; }
  void setDronesOn(bool on)         { _dronesOn = on; }
  void setMasterVolume(float v)     { _master = v; }
  void kickBuzz(float strength)     { _buzzKick = strength; }
  float bowEnvelope() const         { return _bowEnv; }

private:
  static void audioTask(void *arg);
  void render(int16_t *out, int frames);

  volatile float _bow = 0, _melNote = 60, _melLevel = 1, _master = 0.8f, _buzzKick = 0;
  volatile int _bourdon = 48;
  volatile bool _dronesOn = true;
  volatile float _bowEnv = 0;

  // DSP state (audio task only)
  float _ph[4] = {0, 0.3f, 0.6f, 0.9f};
  float _melFreq = 261.6f;
  float _buzzEnv = 0;
  uint32_t _rng = 0x1234567;
  struct Svf { float ic1 = 0, ic2 = 0; } _fMel, _fDrone, _fNoise;
};
