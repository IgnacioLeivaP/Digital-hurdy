#include "player.h"
#include "config.h"
#include <SD.h>
#include <SPI.h>
#include <algorithm>

// ---------------------------------------------------------------------
//  Built-in tunes (public domain). {midi note, length in beat units}, 0 = rest
// ---------------------------------------------------------------------
struct Step { uint8_t note; float beats; };
struct Tune { const char *name; uint16_t bpm; const Step *steps; uint16_t n; };

static const Step ODE[] = {
  {64,1},{64,1},{65,1},{67,1},{67,1},{65,1},{64,1},{62,1},{60,1},{60,1},{62,1},{64,1},{64,1.5f},{62,.5f},{62,2},
  {64,1},{64,1},{65,1},{67,1},{67,1},{65,1},{64,1},{62,1},{60,1},{60,1},{62,1},{64,1},{62,1.5f},{60,.5f},{60,2}};
static const Step FRERE[] = {
  {60,1},{62,1},{64,1},{60,1},{60,1},{62,1},{64,1},{60,1},{64,1},{65,1},{67,2},{64,1},{65,1},{67,2},
  {67,.5f},{69,.5f},{67,.5f},{65,.5f},{64,1},{60,1},{67,.5f},{69,.5f},{67,.5f},{65,.5f},{64,1},{60,1},
  {60,1},{55,1},{60,2},{60,1},{55,1},{60,2}};
static const Step GREEN[] = {
  {69,1},{72,2},{74,1},{76,1.5f},{77,.5f},{76,1},{74,2},{71,1},{67,1.5f},{69,.5f},{71,1},{72,2},{69,1},
  {69,1.5f},{68,.5f},{69,1},{71,2},{68,1},{64,3},{0,1}};

static const Tune TUNES[] = {
  {"Greensleeves", 150, GREEN, sizeof(GREEN) / sizeof(Step)},
  {"Ode to Joy", 110, ODE, sizeof(ODE) / sizeof(Step)},
  {"Frere Jacques", 120, FRERE, sizeof(FRERE) / sizeof(Step)},
};

int Player::builtinCount() { return sizeof(TUNES) / sizeof(TUNES[0]); }

void Player::begin(const Callbacks &cb) { _cb = cb; }

bool Player::mountSd() {
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  _files.clear();
  _sdOk = SD.begin(PIN_SD_CS, SPI, 20000000);
  if (!_sdOk) { Serial.println("[sd] no card / mount failed"); return false; }
  File dir = SD.open(SD_SONG_DIR);
  if (!dir || !dir.isDirectory()) { Serial.println("[sd] /songs not found"); return true; }
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory()) continue;
    String n = f.name();
    String l = n; l.toLowerCase();
    if (l.endsWith(".mid") || l.endsWith(".midi")) _files.push_back(String(SD_SONG_DIR) + "/" + n);
  }
  std::sort(_files.begin(), _files.end(), [](const String &a, const String &b) { return a < b; });
  Serial.printf("[sd] %d MIDI file(s) in %s\n", (int)_files.size(), SD_SONG_DIR);
  return true;
}

const char *Player::songName(int idx) const {
  if (idx < builtinCount()) return TUNES[idx].name;
  idx -= builtinCount();
  return idx < (int)_files.size() ? _files[idx].c_str() : "?";
}

void Player::loadBuiltin(int idx) {
  const Tune &t = TUNES[idx];
  _notes.clear();
  float ms = 60000.0f / t.bpm, pos = 0;
  for (uint16_t i = 0; i < t.n; i++) {
    uint32_t d = (uint32_t)(t.steps[i].beats * ms);
    if (t.steps[i].note) _notes.push_back({(uint32_t)pos, (uint32_t)(d * 0.92f), t.steps[i].note});
    pos += d;
  }
  _name = t.name;
}

bool Player::loadSong(int idx) {
  stop();
  if (idx < 0 || idx >= songCount()) return false;
  if (idx < builtinCount()) {
    loadBuiltin(idx);
  } else {
    const String &path = _files[idx - builtinCount()];
    File f = SD.open(path);
    if (!f) return false;
    size_t len = f.size();
    if (len < 22 || len > 512 * 1024) { f.close(); Serial.println("[sd] bad file size"); return false; }
    uint8_t *buf = (uint8_t *)(psramFound() ? ps_malloc(len) : malloc(len));
    if (!buf) { f.close(); return false; }
    size_t got = f.read(buf, len);
    f.close();
    bool ok = got == len && parseMidi(buf, len);
    free(buf);
    if (!ok) { Serial.printf("[sd] cannot parse %s\n", path.c_str()); return false; }
    _name = path;
  }
  _cur = idx;
  finalizeSong();
  Serial.printf("[player] '%s': %d notes, %u ms, tonic pc=%d\n", _name.c_str(), (int)_notes.size(),
                (unsigned)_endMs, _rootPc);
  return !_notes.empty();
}

// Song length + tonic guess (the last note of a tune is almost always the tonic)
void Player::finalizeSong() {
  _endMs = 0;
  for (auto &n : _notes) _endMs = max(_endMs, n.onMs + n.durMs);
  _rootPc = _notes.empty() ? DEFAULT_ROOT_PC : _notes.back().note % 12;
}

void Player::start(uint32_t nowMs) {
  stop();
  _idx = 0;
  _t0 = nowMs;
  _playing = !_notes.empty();
  _finished = !_playing;
}

void Player::stop() {
  if (_noteActive) { _cb.noteOff(_activeNote); _noteActive = false; }
  _playing = false;
}

void Player::update(uint32_t nowMs) {
  if (!_playing) return;
  uint32_t t = nowMs - _t0;
  if (_noteActive && t >= _notes[_idx - 1].onMs + _notes[_idx - 1].durMs) {
    _cb.noteOff(_activeNote);
    _noteActive = false;
  }
  if (_idx < (int)_notes.size() && t >= _notes[_idx].onMs) {
    if (_noteActive) _cb.noteOff(_activeNote);
    _activeNote = _notes[_idx].note;
    _noteActive = true;
    _cb.noteOn(_activeNote, _idx);
    _idx++;
  }
  if (_idx >= (int)_notes.size() && !_noteActive && t >= _endMs) {
    _playing = false;
    _finished = true;
  }
}

// ---------------------------------------------------------------------
//  Standard MIDI File -> monophonic melody
// ---------------------------------------------------------------------
namespace {
struct Rd {
  const uint8_t *p, *end;
  bool ok = true;
  uint8_t u8() { if (p >= end) { ok = false; return 0; } return *p++; }
  uint16_t u16() { uint16_t a = u8(); return (a << 8) | u8(); }
  uint32_t u32() { uint32_t a = u16(); return (a << 16) | u16(); }
  uint32_t vlq() {
    uint32_t v = 0;
    for (int i = 0; i < 4; i++) { uint8_t b = u8(); v = (v << 7) | (b & 0x7F); if (!(b & 0x80)) break; }
    return v;
  }
  void skip(uint32_t n) { if ((size_t)(end - p) < n) { ok = false; p = end; } else p += n; }
};
struct RawNote { uint32_t tick, endTick; uint8_t note; };
struct Tempo { uint32_t tick, us; };
}  // namespace

bool Player::parseMidi(const uint8_t *data, size_t len) {
  Rd r{data, data + len};
  if (r.u32() != 0x4D546864 /*MThd*/) return false;
  uint32_t hlen = r.u32();
  r.u16();                                   // format (0/1/2 all handled the same)
  uint16_t ntrks = r.u16();
  uint16_t div = r.u16();
  if (!r.ok || (div & 0x8000) || div == 0) return false;
  r.skip(hlen > 6 ? hlen - 6 : 0);

  std::vector<Tempo> tempos;
  std::vector<RawNote> best;
  float bestScore = -1;

  for (uint16_t t = 0; t < ntrks && r.ok && r.p < r.end; t++) {
    uint32_t id = r.u32(), tlen = r.u32();
    if (!r.ok) break;
    const uint8_t *tend = (size_t)(r.end - r.p) < tlen ? r.end : r.p + tlen;
    if (id != 0x4D54726B /*MTrk*/) { r.p = tend; continue; }
    Rd tr{r.p, tend};
    r.p = tend;

    std::vector<RawNote> notes;
    int64_t startTick[128];
    for (auto &s : startTick) s = -1;
    uint32_t tick = 0;
    uint8_t running = 0;
    while (tr.ok && tr.p < tr.end) {
      tick += tr.vlq();
      uint8_t s = tr.u8();
      if (!tr.ok) break;
      if (!(s & 0x80)) { tr.p--; s = running; }            // running status
      else if (s < 0xF0) running = s;
      if (s == 0xFF) {
        uint8_t type = tr.u8();
        uint32_t l = tr.vlq();
        if (type == 0x51 && l == 3) {
          uint32_t us = ((uint32_t)tr.u8() << 16) | ((uint32_t)tr.u8() << 8) | tr.u8();
          tempos.push_back({tick, us});
        } else tr.skip(l);
        if (type == 0x2F) break;
      } else if (s == 0xF0 || s == 0xF7) {
        tr.skip(tr.vlq());
      } else if (s >= 0x80) {
        uint8_t hi = s & 0xF0, ch = s & 0x0F;
        uint8_t d1 = tr.u8();
        uint8_t d2 = (hi == 0xC0 || hi == 0xD0) ? 0 : tr.u8();
        if (ch == 9) continue;                              // skip drums
        bool on = hi == 0x90 && d2 > 0, off = hi == 0x80 || (hi == 0x90 && d2 == 0);
        if (on) {
          if (startTick[d1 & 127] >= 0) notes.push_back({(uint32_t)startTick[d1 & 127], tick, d1});
          startTick[d1 & 127] = tick;
        } else if (off && startTick[d1 & 127] >= 0) {
          notes.push_back({(uint32_t)startTick[d1 & 127], tick, d1});
          startTick[d1 & 127] = -1;
        }
      } else {
        break;                                              // corrupt data
      }
    }
    // Melody track heuristic: enough notes and the highest average pitch
    if (notes.size() >= 8) {
      float sum = 0;
      for (auto &n : notes) sum += n.note;
      float score = sum / notes.size();
      if (score > bestScore) { bestScore = score; best = notes; }
    }
  }
  if (best.empty()) return false;

  // Monophonize: last-note priority, same-tick chords keep the highest note
  std::sort(best.begin(), best.end(), [](const RawNote &a, const RawNote &b) {
    return a.tick != b.tick ? a.tick < b.tick : a.note < b.note;
  });
  std::sort(tempos.begin(), tempos.end(), [](const Tempo &a, const Tempo &b) { return a.tick < b.tick; });
  if (tempos.empty() || tempos[0].tick > 0) tempos.insert(tempos.begin(), {0, 500000});

  auto toMs = [&](uint32_t tick) -> uint32_t {
    double ms = 0;
    uint32_t segTick = 0, us = tempos[0].us;
    for (auto &tp : tempos) {
      if (tp.tick >= tick) break;
      ms += (double)(tp.tick - segTick) * us / div / 1000.0;
      segTick = tp.tick; us = tp.us;
    }
    return (uint32_t)(ms + (double)(tick - segTick) * us / div / 1000.0);
  };

  _notes.clear();
  for (size_t i = 0; i < best.size() && _notes.size() < 6000; i++) {
    uint32_t end = best[i].endTick;
    if (i + 1 < best.size() && best[i + 1].tick < end) end = best[i + 1].tick;
    if (end <= best[i].tick) continue;
    uint32_t on = toMs(best[i].tick), off = toMs(end);
    if (off < on + 30) continue;
    _notes.push_back({on, (uint32_t)((off - on) * 0.95f), best[i].note});
  }
  return !_notes.empty();
}
