#pragma once
#include <Arduino.h>

// =====================================================================
//  Digital Hurdy-Gurdy — hardware and tuning configuration
//  Board: ESP32-S3 DevKitC-1 N16R8 (16 MB flash, 8 MB OCTAL PSRAM)
//  Do NOT use GPIO 26-37 (flash/PSRAM), 19/20 (native USB) or 0/3/45/46
//  (strapping pins).
// =====================================================================

// ---- Audio: I2S -> PCM5102A DAC --------------------------------------
constexpr uint32_t SAMPLE_RATE = 32000;
constexpr int PIN_I2S_BCK  = 5;   // PCM5102A BCK
constexpr int PIN_I2S_WS   = 6;   // PCM5102A LCK (LRCK)
constexpr int PIN_I2S_DOUT = 7;   // PCM5102A DIN
// PCM5102A: SCK -> GND (internal PLL), FMT -> GND, XSMT -> 3V3, FLT -> GND

// ---- MicroSD (SPI) ---------------------------------------------------
constexpr int PIN_SD_CS   = 10;
constexpr int PIN_SD_MOSI = 11;
constexpr int PIN_SD_SCK  = 12;
constexpr int PIN_SD_MISO = 13;
#define SD_SONG_DIR "/songs"

// ---- Crank: E38S6G5-600B incremental encoder (600 PPR) ---------------
constexpr int PIN_ENC_A = 1;
constexpr int PIN_ENC_B = 2;
constexpr int PIN_ENC_Z = 42;     // index pulse (once per revolution)
constexpr int ENC_PPR = 600;
constexpr int ENC_COUNTS_PER_REV = ENC_PPR * 4;   // x4 quadrature decoding
// If the crank spins "backwards" it is only cosmetic (sound is symmetric),
// but the sign is exposed in Crank::direction().

// Crank speed (revolutions per second) -> bow pressure
constexpr float CRANK_MIN_RPS  = 0.20f;   // below this the wheel is silent
constexpr float CRANK_FULL_RPS = 2.20f;   // at/above this the wheel is at full bow
// "Coup de poignet": a quick crank acceleration makes the buzzing bridge speak
constexpr float JERK_THRESHOLD_RPS = 0.60f;
constexpr uint32_t JERK_COOLDOWN_MS = 90;

// ---- Keys (to GND, internal pull-up) ---------------------------------
constexpr uint8_t KEY_PINS[] = {8, 9, 14, 15, 16, 17, 18, 21, 38, 39, 40, 41};
constexpr int NUM_KEYS = sizeof(KEY_PINS) / sizeof(KEY_PINS[0]);
// Semitones above the open string for each key (default: major scale, ~1.7 oct).
// For a chromatic keybox use {0,1,2,...,11}.
constexpr int8_t KEY_SEMITONES[NUM_KEYS] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19};

// Outemu (Cherry-style) mechanical switches bounce up to ~5 ms
constexpr uint32_t KEY_DEBOUNCE_MS = 7;

// ---- Buttons / misc --------------------------------------------------
constexpr int PIN_MODE_BTN = 47;   // short: PLAY<->AUTO, long: next song / new tonic
constexpr int PIN_BATT_ADC = 4;    // battery through 100k/100k divider (optional)
constexpr uint32_t LONG_PRESS_MS = 700;
constexpr float BATT_LOW_V = 3.30f;

// ---- Musical defaults ------------------------------------------------
constexpr uint8_t DEFAULT_ROOT_PC = 0;   // 0 = C, 2 = D, 7 = G ...
constexpr int OPEN_STRING_BASE = 60;     // MIDI note of the open melody string (C4) + root pc
constexpr uint8_t MIDI_CHANNEL = 1;

// ---- Auto-play -------------------------------------------------------
constexpr float AUTO_BOW = 0.70f;          // virtual crank pressure when playing alone
constexpr int AUTO_BUZZ_EVERY = 3;         // buzz accent every N notes
constexpr uint32_t AUTO_SONG_GAP_MS = 1800;
constexpr bool AUTO_ADVANCE = true;        // go to next song when one ends
