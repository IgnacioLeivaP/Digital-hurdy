// Digital Hurdy-Gurdy — ESP32-S3
//   Crank (encoder) + keys -> built-in hurdy-gurdy synth -> I2S PCM5102A -> PAM8403.
//   Also a USB-MIDI device (out: keys/crank, in: play notes from a computer/phone)
//   and it can play by itself (built-in tunes + .mid files on the SD card).
#include <Arduino.h>
#include <USB.h>
#include <USBMIDI.h>
#include "config.h"
#include "crank.h"
#include "keys.h"
#include "synth.h"
#include "player.h"

static_assert(ARDUINO_USB_MODE == 0, "USB MIDI needs ARDUINO_USB_MODE=0 (TinyUSB); see platformio.ini");

static HurdySynth synth;
static Crank crank;
static Keys keys;
static Button modeBtn;
static Player player;
static USBMIDI midi;

enum Mode { MODE_PLAY, MODE_AUTO };
static Mode mode = MODE_PLAY;

static int rootPc = DEFAULT_ROOT_PC;     // tonic: open string + drones
static float virtualBow = 0;             // AUTO mode or MIDI CC1
static float midiBow = 0;
static float autoNoteLevel = 1;

// Notes coming from USB MIDI (last-note priority stack)
static uint8_t midiHeld[16];
static int midiHeldCount = 0;
// Note currently sent to the synth by the sequencer
static int autoNote = -1;
static int lastSentKeyNote = -1;
static uint32_t lastCcMs = 0;
static int lastCcVal = -1;

static void applyRoot(int pc) {
  rootPc = ((pc % 12) + 12) % 12;
  synth.setDroneRoot(48 + rootPc);      // bourdon in C3..B3, mouche a fifth above
}

// ---- sequencer callbacks ---------------------------------------------
static void onAutoNoteOn(uint8_t note, uint32_t index) {
  autoNote = note;
  autoNoteLevel = 0.85f + 0.15f * ((index % 4) == 0);
  if (index % AUTO_BUZZ_EVERY == 0) synth.kickBuzz(0.7f);
  midi.noteOn(note, 100, MIDI_CHANNEL);
}
static void onAutoNoteOff(uint8_t note) {
  autoNote = -1;
  midi.noteOff(note, 0, MIDI_CHANNEL);
}

static void startSong(int idx) {
  if (player.loadSong(idx)) {
    applyRoot(player.rootPc());
    player.start(millis());
  } else {
    // skip unreadable songs
    Serial.println("[main] song load failed, trying next");
    idx = (idx + 1) % player.songCount();
    if (idx != player.current() && player.loadSong(idx)) {
      applyRoot(player.rootPc());
      player.start(millis());
    }
  }
}

static void setMode(Mode m) {
  mode = m;
  if (m == MODE_AUTO) {
    Serial.println("[main] AUTO mode");
    startSong(player.current() < 0 ? 0 : player.current());
    virtualBow = AUTO_BOW;
  } else {
    Serial.println("[main] PLAY mode");
    player.stop();
    autoNote = -1;
    virtualBow = 0;
    applyRoot(DEFAULT_ROOT_PC);
  }
}

// ---- USB MIDI input --------------------------------------------------
static void heldRemove(uint8_t n) {
  for (int i = 0; i < midiHeldCount; i++)
    if (midiHeld[i] == n) {
      memmove(&midiHeld[i], &midiHeld[i + 1], midiHeldCount - i - 1);
      midiHeldCount--;
      return;
    }
}

static void pollUsbMidi() {
  midiEventPacket_t p;
  while (midi.readPacket(&p)) {
    uint8_t st = p.byte1 & 0xF0, d1 = p.byte2, d2 = p.byte3;
    if (st == 0x90 && d2 > 0) {
      heldRemove(d1);
      if (midiHeldCount < (int)sizeof(midiHeld)) midiHeld[midiHeldCount++] = d1;
    } else if (st == 0x80 || (st == 0x90 && d2 == 0)) {
      heldRemove(d1);
    } else if (st == 0xB0) {
      if (d1 == 1) midiBow = d2 / 127.0f;                            // mod wheel = turn the crank
      else if (d1 == 7) synth.setMasterVolume(0.9f * d2 / 127.0f);   // volume
      else if (d1 == 20) applyRoot(d2);                              // tonic (pitch class)
      else if (d1 == 123) midiHeldCount = 0;                         // all notes off
    }
  }
}

// ---- battery ---------------------------------------------------------
static void checkBattery(uint32_t now) {
  static uint32_t last = 0;
  if (now - last < 15000) return;
  last = now;
  float v = analogReadMilliVolts(PIN_BATT_ADC) * 2.0f / 1000.0f;   // 100k/100k divider
  if (v > 1.0f) {                                                  // ignore if not wired
    Serial.printf("[batt] %.2f V%s\n", v, v < BATT_LOW_V ? "  LOW - recharge" : "");
  }
}

void setup() {
  Serial.begin(115200);       // UART0 (the "UART" USB-C port), USB port is MIDI
  delay(200);
  Serial.println("\n== Digital Hurdy-Gurdy ==");

  midi.begin();
  USB.begin();

  keys.begin();
  modeBtn.begin(PIN_MODE_BTN);
  crank.begin();
  analogReadResolution(12);

  player.begin({onAutoNoteOn, onAutoNoteOff});
  player.mountSd();
  Serial.printf("[main] %d song(s) available\n", player.songCount());

  applyRoot(DEFAULT_ROOT_PC);
  synth.setMasterVolume(0.8f);
  synth.setMelodyNote(OPEN_STRING_BASE + rootPc);
  synth.begin();
}

void loop() {
  uint32_t now = millis();

  crank.update(now);
  keys.update(now);
  pollUsbMidi();

  switch (modeBtn.update(now)) {
    case Button::SHORT_PRESS:
      setMode(mode == MODE_PLAY ? MODE_AUTO : MODE_PLAY);
      break;
    case Button::LONG_PRESS:
      if (mode == MODE_AUTO) startSong((player.current() + 1) % player.songCount());
      else applyRoot(rootPc + 7);      // circle of fifths: C, G, D, A, E ...
      break;
    default: break;
  }

  if (mode == MODE_AUTO) {
    player.update(now);
    if (player.finished() && AUTO_ADVANCE) {
      static uint32_t endedAt = 0;
      if (!endedAt) endedAt = now;
      if (now - endedAt > AUTO_SONG_GAP_MS) {
        endedAt = 0;
        startSong((player.current() + 1) % player.songCount());
      }
    }
  }

  // ---- decide melody pitch: MIDI in > sequencer > keys > open string ----
  float note = OPEN_STRING_BASE + rootPc;
  float level = 1.0f;
  int keyNote = -1;
  if (midiHeldCount > 0) {
    note = midiHeld[midiHeldCount - 1];
  } else if (mode == MODE_AUTO && autoNote >= 0) {
    note = autoNote;
    level = autoNoteLevel;
  } else if (keys.semitone() >= 0) {
    keyNote = OPEN_STRING_BASE + rootPc + keys.semitone();
    note = keyNote;
  }
  synth.setMelodyNote(note);
  synth.setMelodyLevel(level);

  // Keys -> MIDI out (only in PLAY mode, so AUTO doesn't double-send)
  if (mode == MODE_PLAY && keyNote != lastSentKeyNote) {
    if (lastSentKeyNote >= 0) midi.noteOff(lastSentKeyNote, 0, MIDI_CHANNEL);
    if (keyNote >= 0) midi.noteOn(keyNote, 100, MIDI_CHANNEL);
    lastSentKeyNote = keyNote;
  }

  // ---- bow: real crank, virtual crank (AUTO) or MIDI CC1, whichever is highest ----
  float bow = fmaxf(crank.bow(), fmaxf(virtualBow, midiBow));
  synth.setBow(bow);

  float js;
  if (crank.consumeJerk(js)) synth.kickBuzz(js);

  // Crank speed as MIDI CC11 (expression), throttled
  int cc = (int)(crank.bow() * 127.0f);
  if (cc != lastCcVal && now - lastCcMs > 30) {
    midi.controlChange(11, cc, MIDI_CHANNEL);
    lastCcVal = cc;
    lastCcMs = now;
  }

  checkBattery(now);
  delay(1);
}
