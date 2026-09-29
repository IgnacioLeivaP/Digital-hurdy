#pragma once
#include "Arduino.h"
enum i2s_mode_t { I2S_MODE_STD }; enum i2s_data_bit_width_t { I2S_DATA_BIT_WIDTH_16BIT }; enum i2s_slot_mode_t { I2S_SLOT_MODE_STEREO };
struct I2SClass { void setPins(int, int, int) {} bool begin(i2s_mode_t, uint32_t, i2s_data_bit_width_t, i2s_slot_mode_t) { return true; } size_t write(const uint8_t *, size_t n) { return n; } };
