#pragma once
#include "Arduino.h"
struct File { operator bool() const { return false; } bool isDirectory() { return false; } File openNextFile() { return {}; }
  const char *name() { return ""; } size_t size() { return 0; } size_t read(uint8_t *, size_t) { return 0; } void close() {} };
struct SDStub { template <class T> bool begin(int, T &, int) { return false; } File open(const String &) { return {}; } };
static SDStub SD;
