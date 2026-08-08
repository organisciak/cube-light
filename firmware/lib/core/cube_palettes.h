#pragma once
#include <cstdint>

#include "cube_params.h"

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
  bool solo = false;         // single-color mode: ignore the sampled t
  float soloT = 0;           // palette position every voxel uses when solo
};

/** All selectable palette names, "none" first (mirrors TS paletteNames). */
extern const char* const kPaletteNames[];
extern const int kPaletteNameCount;

/** True when `name` names a real palette (i.e. not "none"/unknown). */
bool paletteActive(const char* name);

/**
 * Resolve by name; unknown names and "none" fall back to rainbow, mirroring
 * the TS getPalette() behavior. (Use paletteActive() to decide whether the
 * pattern should use its own RGB/HSV logic instead.)
 */
PaletteRef resolvePalette(const char* name);

/**
 * Same, plus the shared single-color ("solo") mode read from two params every
 * palette-bearing pattern carries (see CUBE_PALETTE_SOLO_SPECS):
 *
 *   paletteSolo  bool — the whole pattern takes ONE palette color per frame
 *   soloSpeed    how fast that color drifts through the palette (sweeps/s)
 *
 * With it on, samplePalette() ignores the per-voxel t entirely and returns the
 * drifting color, so a pattern that normally spreads "rainbow" across the cube
 * instead walks through the rainbow one color at a time. `now` is the
 * pattern clock (ctx.t) that drives the drift.
 */
PaletteRef resolvePalette(const char* name, const Params& params, float now);

/**
 * Sample at t in [0,1]. `now` is a seconds wall-clock used only by the
 * "cycle" palette's crossfade (on-chip: seconds since boot — the cycle
 * phase drifting per boot is fine). In solo mode t is ignored (see above).
 */
void samplePalette(const PaletteRef& ref, float t, float now, uint8_t out[3]);

}  // namespace cube
