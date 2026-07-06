#pragma once
#include <cstdlib>

// Math.random() stand-in. rand() is fine on both host and ESP32 for visual
// jitter; nothing here needs cryptographic or even statistical quality.

namespace cube {

inline float frand() { return (float)std::rand() / ((float)RAND_MAX + 1.0f); }

}  // namespace cube
