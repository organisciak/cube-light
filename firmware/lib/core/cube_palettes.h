#pragma once
#include <cstdint>

// Port of src/shared/palettes.ts. Gradient stop tables live in flash/rodata.
// Resolve a palette name once per frame with resolvePalette(), then sample
// per voxel with sample() — avoids strcmp in the inner loop.

namespace cube {

struct PaletteStop {
  float pos;
  uint8_t r, g, b;
};

enum class PaletteKind : uint8_t {
  Gradient,
  Rainbow,  // hsv ring
  Cycle,    // auto-crossfades through the gradient list using wall time
};

struct PaletteRef {
  PaletteKind kind;
  const PaletteStop* stops;  // Gradient only
  int stopCount;             // Gradient only
};

/** True when `name` names a real palette (i.e. not "none"/unknown). */
bool paletteActive(const char* name);

/**
 * Resolve by name; unknown names and "none" fall back to rainbow, mirroring
 * the TS getPalette() behavior. (Use paletteActive() to decide whether the
 * pattern should use its own RGB/HSV logic instead.)
 */
PaletteRef resolvePalette(const char* name);

/**
 * Sample at t in [0,1]. `now` is a seconds wall-clock used only by the
 * "cycle" palette's crossfade (on-chip: seconds since boot — the cycle
 * phase drifting per boot is fine).
 */
void samplePalette(const PaletteRef& ref, float t, float now, uint8_t out[3]);

}  // namespace cube
