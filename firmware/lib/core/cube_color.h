#pragma once
#include <cstdint>

// Port of src/shared/color.ts.

namespace cube {

/** HSV (each in 0..1) -> RGB in 0..255. Matches the TS implementation. */
void hsvToRgb(float h, float s, float v, uint8_t out[3]);

inline float clamp01(float n) { return n < 0 ? 0 : n > 1 ? 1 : n; }

}  // namespace cube
