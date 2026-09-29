#include "synth.h"
#include "config.h"
#include <ESP_I2S.h>
#include <math.h>

static I2SClass i2s;
static constexpr int BLOCK = 64;

static inline float midiToHz(float n) { return 440.0f * powf(2.0f, (n - 69.0f) / 12.0f); }

// PolyBLEP band-limited sawtooth (phase 0..1, increment dt)
static inline float polyblep(float t, float dt) {
  if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
  if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
  return 0.0f;
}
static inline float saw(float &ph, float dt) {
  float v = 2.0f * ph - 1.0f - polyblep(ph, dt);
  ph += dt;
  if (ph >= 1.0f) ph -= 1.0f;
  return v;
}

// Zero-delay-feedback state variable filter (Zavalishin/Cytomic)
struct SvfCoef { float a1, a2, a3, k; };
static inline SvfCoef svfCoef(float fc, float q) {
  float g = tanf(M_PI * fminf(fc, SAMPLE_RATE * 0.45f) / SAMPLE_RATE);
  float k = 1.0f / q;
  float a1 = 1.0f / (1.0f + g * (g + k));
  return {a1, g * a1, g * g * a1, k};
}
template <typename S>
static inline void svfRun(S &s, const SvfCoef &c, float in, float &lp, float &bp) {
  float v3 = in - s.ic2;
  float v1 = c.a1 * s.ic1 + c.a2 * v3;
  float v2 = s.ic2 + c.a2 * s.ic1 + c.a3 * v3;
  s.ic1 = 2.0f * v1 - s.ic1;
  s.ic2 = 2.0f * v2 - s.ic2;
  lp = v2;
  bp = v1;
}

void HurdySynth::begin() {
  i2s.setPins(PIN_I2S_BCK, PIN_I2S_WS, PIN_I2S_DOUT);
  if (!i2s.begin(I2S_MODE_STD, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
    Serial.println("[synth] I2S init FAILED");
    return;
  }
  xTaskCreatePinnedToCore(audioTask, "audio", 6144, this, 6, nullptr, 1);
}

void HurdySynth::audioTask(void *arg) {
  auto *self = static_cast<HurdySynth *>(arg);
  int16_t buf[BLOCK * 2];
  for (;;) {
    self->render(buf, BLOCK);
    i2s.write((uint8_t *)buf, sizeof(buf));
  }
}

void HurdySynth::render(int16_t *out, int frames) {
  const float fs = SAMPLE_RATE;
  const float bowTarget = _bow;
  const float melTarget = midiToHz(_melNote);
  const float bourdonHz = midiToHz(_bourdon);
  const float moucheHz = bourdonHz * 1.5f;       // fifth above the bourdon
  const bool drones = _dronesOn;
  const float master = _master;
  const float melLevel = _melLevel;

  if (_buzzKick > 0) { _buzzEnv = fmaxf(_buzzEnv, _buzzKick); _buzzKick = 0; }

  // Envelope / glide coefficients (one-pole)
  static const float atk = 1.0f - expf(-1.0f / (fs * 0.030f));
  static const float rel = 1.0f - expf(-1.0f / (fs * 0.140f));
  static const float glide = 1.0f - expf(-1.0f / (fs * 0.004f));
  static const float buzzDecay = expf(-1.0f / (fs * 0.070f));

  // Block-rate filter coefficients from the current bow envelope
  float env = _bowEnv;
  SvfCoef cMel = svfCoef(450.0f + 6500.0f * env, 0.9f);
  SvfCoef cDrone = svfCoef(220.0f + 1300.0f * env, 0.8f);
  SvfCoef cNoise = svfCoef(2300.0f, 2.5f);
  const float det = 0.0022f;                        // ~ +/-4 cents unison detune

  for (int i = 0; i < frames; i++) {
    env += (bowTarget - env) * (bowTarget > env ? atk : rel);
    _melFreq += (melTarget - _melFreq) * glide;

    float dt = _melFreq / fs;
    float mel = 0.5f * (saw(_ph[0], dt * (1.0f + det)) + saw(_ph[1], dt * (1.0f - det)));
    float dr = 0;
    if (drones) dr = 0.6f * saw(_ph[2], bourdonHz / fs) + 0.4f * saw(_ph[3], moucheHz / fs);

    _rng ^= _rng << 13; _rng ^= _rng >> 17; _rng ^= _rng << 5;
    float noise = ((int32_t)_rng) * (1.0f / 2147483648.0f);
    float nlp, nbp;
    svfRun(_fNoise, cNoise, noise, nlp, nbp);

    float lp, bp;
    svfRun(_fMel, cMel, mel, lp, bp);
    float melOut = lp;
    svfRun(_fDrone, cDrone, dr, lp, bp);
    float droneOut = lp;

    _buzzEnv *= buzzDecay;
    float buzz = nbp * _buzzEnv * 1.6f;             // trompette rattle
    float rosin = nbp * 0.05f * env;                // wheel scrape

    float loud = env * env * (3.0f - 2.0f * env);   // smoothstep: soft at low pressure
    float x = melOut * 0.55f * melLevel * loud + droneOut * 0.30f * loud +
              (buzz + rosin) * (env > 0.02f ? 1.0f : 0.0f) * 0.5f;
    x *= master;
    x = x / (1.0f + fabsf(x));                      // soft clip
    int16_t s = (int16_t)(x * 30000.0f);
    out[2 * i] = s;
    out[2 * i + 1] = s;
  }
  _bowEnv = env;
}
