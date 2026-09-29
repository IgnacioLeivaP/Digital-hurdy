// Minimal Arduino stub so synth.cpp / player.cpp build and run on a PC.
#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <algorithm>
#include <cstdarg>
#define IRAM_ATTR
using std::max; using std::min;
#include "freertos_stub.h"
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))
inline bool psramFound() { return false; }
inline void *ps_malloc(size_t n) { return malloc(n); }
class String : public std::string {
public:
  String() {}
  String(const char *s) : std::string(s) {}
  String(const std::string &s) : std::string(s) {}
  void toLowerCase() { for (auto &c : *this) c = tolower(c); }
  bool endsWith(const char *e) const { size_t l = strlen(e); return size() >= l && compare(size() - l, l, e) == 0; }
};
struct SerialStub {
  void println(const char *s) { puts(s); }
  void printf(const char *f, ...) { va_list a; va_start(a, f); vprintf(f, a); va_end(a); }
};
static SerialStub Serial;
inline uint32_t millis() { return 0; }
