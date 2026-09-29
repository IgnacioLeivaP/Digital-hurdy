// Host-side checks: renders the synth to WAV and parses a synthetic MIDI file.
#include "config.h"
#define private public
#include "synth.h"
#include "player.h"
#undef private
#include <vector>
#include <fstream>
#include <complex>
#include <cstdio>

static void writeWav(const char *path, const std::vector<int16_t> &mono, int fs) {
  FILE *f = fopen(path, "wb");
  uint32_t dataLen = mono.size() * 2, rate = fs, br = fs * 2;
  uint16_t fmt = 1, ch = 1, ba = 2, bits = 16;
  uint32_t riff = 36 + dataLen, fl = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); fwrite(&fl, 4, 1, f);
  fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&rate, 4, 1, f); fwrite(&br, 4, 1, f);
  fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&dataLen, 4, 1, f);
  fwrite(mono.data(), 2, mono.size(), f); fclose(f);
}

// crude pitch detector: autocorrelation on the last 4096 samples
static float detectHz(const std::vector<int16_t> &m, int fs) {
  size_t n = 4096, s = m.size() - n; float best = 0; int bestLag = 0;
  for (int lag = fs / 1000; lag < fs / 60; lag++) {
    double acc = 0; for (size_t i = 0; i < n - lag; i++) acc += (double)m[s + i] * m[s + i + lag];
    if (acc > best) { best = acc; bestLag = lag; }
  }
  return bestLag ? (float)fs / bestLag : 0;
}

int main() {
  const int fs = SAMPLE_RATE;
  HurdySynth syn;
  std::vector<int16_t> out;
  int16_t blk[128];
  auto run = [&](float sec) { for (int i = 0; i < sec * fs / 64; i++) { syn.render(blk, 64); for (int k = 0; k < 64; k++) out.push_back(blk[2 * k]); } };
  auto peak = [&](size_t from) { int p = 0; for (size_t i = from; i < out.size(); i++) p = std::max(p, abs((int)out[i])); return p; };

  syn.setDroneRoot(48);
  syn.setBow(0); syn.setMelodyNote(60); run(0.3f);
  printf("silent wheel: peak=%d (expect ~0)\n", peak(0));

  // Melody only (drones off) to check pitch tracking
  syn.setDronesOn(false); syn.setBow(1.0f);
  for (int n : {60, 64, 67, 72}) {
    syn.setMelodyNote(n); run(0.6f);
    float hz = detectHz(out, fs), want = 440.0f * powf(2, (n - 69) / 12.0f);
    printf("note %d: want %.1f Hz, got %.1f Hz, peak=%d\n", n, want, hz, peak(out.size() - 4096));
  }
  // Full instrument demo: drones + melody + buzz kicks
  out.clear(); syn.setDronesOn(true);
  const int mel[] = {60, 62, 64, 65, 67, 65, 64, 62, 60};
  syn.setBow(0.0f); run(0.2f);
  for (int i = 0; i < 9; i++) { syn.setBow(0.75f); syn.setMelodyNote(mel[i]); if (i % 3 == 0) syn.kickBuzz(0.8f); run(0.4f); }
  syn.setBow(0); run(0.8f);
  writeWav("demo.wav", out, fs);
  bool bad = false; for (auto v : out) if (v == INT16_MIN) bad = true;
  printf("demo.wav: %zu samples, peak=%d, clipping=%s\n", out.size(), peak(0), bad ? "YES" : "no");

  // ---- MIDI parser: format 1, 2 tracks, tempo change, running status, chord, drums ----
  std::vector<uint8_t> f = {'M','T','h','d',0,0,0,6,0,1,0,3,0x01,0xE0};   // 480 tpq, 3 tracks
  auto trk = [&](std::vector<uint8_t> d) {
    f.insert(f.end(), {'M','T','r','k',0,0,0,(uint8_t)d.size()}); f.insert(f.end(), d.begin(), d.end()); };
  trk({0x00,0xFF,0x51,0x03,0x07,0xA1,0x20, 0x83,0x60,0xFF,0x51,0x03,0x0F,0x42,0x40, 0x00,0xFF,0x2F,0x00}); // 120bpm -> 60bpm at tick 480
  // bass track (low notes) - must be ignored in favour of the melody track
  std::vector<uint8_t> bass; for (int i = 0; i < 8; i++) { bass.insert(bass.end(), {0x00,0x90,36,90, 0x81,0x70,0x80,36,0}); }
  bass.insert(bass.end(), {0x00,0xFF,0x2F,0x00}); trk(bass);
  // melody: running status, C4 D4 E4 then a chord (E4+G4 same tick), tail notes
  std::vector<uint8_t> m = {0x00,0x90,60,100, 0x83,0x60,0x80,60,0, 0x00,0x90,62,100, 0x83,0x60,0x80,62,0};
  m.insert(m.end(), {0x00,0x90,64,100, 0x00,67,100, 0x83,0x60,0x80,64,0, 0x00,67,0});   // running-status note-on + vel0 off
  for (int i = 0; i < 6; i++) m.insert(m.end(), {0x00,0x90,(uint8_t)(69+i),100, 0x60,0x80,(uint8_t)(69+i),0});
  m.insert(m.end(), {0x00,0x99,38,100, 0x60,0x89,38,0});                                // drum, must be ignored
  m.insert(m.end(), {0x00,0xFF,0x2F,0x00}); trk(m);

  Player p; bool ok = p.parseMidi(f.data(), f.size());
  printf("parseMidi ok=%d, notes=%zu\n", ok, p.notes().size());
  for (auto &n : p.notes()) printf("  on=%5u ms dur=%4u ms note=%u\n", n.onMs, n.durMs, n.note);
  // truncated / garbage input must not crash
  for (size_t cut = 0; cut < f.size(); cut += 3) { Player q; q.parseMidi(f.data(), cut); }
  std::vector<uint8_t> junk(200); for (auto &b : junk) b = rand(); Player q; printf("garbage ok=%d\n", q.parseMidi(junk.data(), junk.size()));
  return 0;
}
