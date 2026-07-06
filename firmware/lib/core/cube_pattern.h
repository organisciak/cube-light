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
};

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
