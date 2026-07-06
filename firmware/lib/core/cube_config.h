#pragma once
#include <cstdint>

// Mirror of src/shared/types.ts constants. The core library under lib/core is
// pure C++ (no Arduino dependencies) so the exact same code compiles for the
// ESP32 target and for the native host harness used to preview patterns.

namespace cube {

constexpr int CUBE_N = 10;
constexpr int NUM_LEDS = CUBE_N * CUBE_N * CUBE_N;
constexpr int AUDIO_BANDS = 8;

}  // namespace cube
