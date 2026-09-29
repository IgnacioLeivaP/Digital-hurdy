#pragma once
#include <Arduino.h>
#include <vector>

// Monophonic melody sequencer so the instrument can play by itself.
// Songs come from built-in tunes and from *.mid files in /songs on the SD card.
class Player {
public:
  struct NoteEvt {
    uint32_t onMs;    // start, relative to song start
    uint32_t durMs;
    uint8_t note;
  };
  struct Callbacks {
    void (*noteOn)(uint8_t note, uint32_t index);
    void (*noteOff)(uint8_t note);
  };

  void begin(const Callbacks &cb);
  bool mountSd();                       // scans SD_SONG_DIR
  int  songCount() const { return builtinCount() + (int)_files.size(); }
  const char *songName(int idx) const;
  bool loadSong(int idx);               // parses and prepares the song
  void start(uint32_t nowMs);           // (re)start current song from the top
  void stop();                          // silences the current note
  void update(uint32_t nowMs);          // call often
  bool finished() const { return _finished; }
  int  current() const { return _cur; }
  int  rootPc() const { return _rootPc; }   // detected tonic (0..11)
  bool sdOk() const { return _sdOk; }

  // Exposed for unit-style tests on the host
  bool parseMidi(const uint8_t *data, size_t len);
  const std::vector<NoteEvt> &notes() const { return _notes; }

private:
  static int builtinCount();
  void loadBuiltin(int idx);
  void finalizeSong();

  Callbacks _cb{};
  std::vector<NoteEvt> _notes;
  std::vector<String> _files;
  String _name;
  int _cur = -1;
  int _rootPc = 0;
  bool _sdOk = false;
  bool _playing = false, _finished = true;
  int _idx = 0;
  bool _noteActive = false;
  uint8_t _activeNote = 0;
  uint32_t _t0 = 0, _endMs = 0;
};
