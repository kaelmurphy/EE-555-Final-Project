#pragma once
#include <cstdint>

// From ITU-T H.264 / AVC CABAC tables,
// also used in x264, ffmpeg, and openHEVC.
// These tables are public domain.

extern const uint8_t cabacRangeTabLPS[64][4];
extern const uint8_t cabacTransIdxLPS[64];
extern const uint8_t cabacTransIdxMPS[64];