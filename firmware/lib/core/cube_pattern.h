#pragma once
#include <cstdint>

#include "cube_config.h"
#include "cube_geometry.h"
#include "cube_params.h"

// Port of src/shared/patterns/types.ts. Kept deliberately close to the TS
// shape so pattern ports are line-by-line translations.

namespace cube {

struct AudioFrame {
  float level = 0;              // overall level 0..1
  float bands[AUDIO_BANDS] = {0};  // log-spaced bands 0..1
  float beat = 0;               // beat envelope 0..1
  float bpm = 0;                // tempo estimate; 0 = no lock
};

// Shared semantics for the "speedFrom"/"speedGain" param pair that several
// patterns use to tie their motion rate to the music:
//   none  -> 1
//   level -> 1 + gain * level
//   bpm   -> 1 + gain * (bpm / 120), so gain=1 doubles speed at 120 BPM;
//            no tempo lock (bpm 0) leaves speed unchanged.
inline float audioSpeedMult(const AudioFrame& a, const char* from, float gain) {
  if (from[0] == 'l') return 1.0f + gain * a.level;
  if (from[0] == 'b') return a.bpm > 0 ? 1.0f + gain * (a.bpm / 120.0f) : 1.0f;
  return 1.0f;
}

struct PatternCtx {
  uint8_t* buffer;        // NUM_LEDS * 3 RGB triples, mutate in place
  const Geometry* geo;
  float t;                // seconds since pattern start
  float dt;               // seconds since previous frame
  const AudioFrame* audio;
  const Params* params;

  uint16_t idx(int x, int y, int z) const { return geo->idx(x, y, z); }
};

struct Pattern {
  const char* id;
  void (*init)(PatternCtx&);    // nullable; called once on select
  void (*render)(PatternCtx&);  // called every frame, fills ctx.buffer
};

/** Registry (port of patterns/index.ts). */
const Pattern* findPattern(const char* id);
extern const Pattern* const kPatterns[];
extern const int kPatternCount;
extern const char* const kDefaultPatternId;

}  // namespace cube
