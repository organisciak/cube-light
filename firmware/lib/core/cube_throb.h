#pragma once
#include <cmath>

#include "cube_color.h"
#include "cube_pattern.h"

// Shared music-reactive brightness throb (snake body, text, album art, ...).
//
// The raw BeatDetector envelope snaps to 1 on an onset and decays over a
// fixed 0.25s, which reads twitchy when used directly as a brightness
// multiplier. This wraps it in asymmetric attack/release smoothing so each
// pattern can tune how snappy or lazy the pulse feels.
//
// Shared param vocabulary (see CUBE_THROB_SPECS in cube_param_specs.h):
//   throbDepth   0..0.8  how far below full brightness the lull sits; 0 = off
//   throbAttack  seconds for a beat to flash up to full (0 = instant snap)
//   throbRelease seconds to sink back down after the beat passes

namespace cube {

struct AudioThrob {
  float env = 0;  // smoothed beat energy 0..1

  void reset() { env = 0; }

  /**
   * Advance the smoothed envelope and return it (0..1). Patterns that drive
   * something other than brightness — orbit's particle size, say — call this
   * directly; update() layers the brightness mapping on top. Runs even when
   * throb depth is 0 so the envelope is already warm if it's dialed up.
   */
  float advance(const AudioFrame& audio, float dt, float attackS,
                float releaseS) {
    const float target = audio.beat;
    const float tau = target > env ? attackS : releaseS;
    if (tau <= 0.005f) {
      env = target;
    } else {
      env += (target - env) * (1.0f - std::exp(-dt / tau));
    }
    if (env < 0) env = 0;
    if (env > 1) env = 1;
    return env;
  }

  /**
   * Advance the envelope and return a brightness multiplier in
   * [1-depth .. 1]. Call once per frame with the pattern's dt.
   */
  float update(const AudioFrame& audio, float dt, float depth, float attackS,
               float releaseS) {
    advance(audio, dt, attackS, releaseS);
    if (depth <= 0) return 1.0f;
    return 1.0f - depth * (1.0f - env);
  }

  /** Read params by the shared keys and update. */
  float update(const PatternCtx& ctx) {
    const Params& p = *ctx.params;
    return update(*ctx.audio, ctx.dt, clamp01(p.num("throbDepth", 0.0f)),
                  p.num("throbAttack", 0.03f), p.num("throbRelease", 0.5f));
  }
};

}  // namespace cube
