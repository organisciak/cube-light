// Port of src/shared/patterns/solid.ts.
#include <cmath>

#include "cube_pattern.h"

namespace cube {
namespace {

void render(PatternCtx& ctx) {
  const uint8_t r = (uint8_t)ctx.params->num("r", 64);
  const uint8_t g = (uint8_t)ctx.params->num("g", 64);
  const uint8_t b = (uint8_t)ctx.params->num("b", 64);
  for (int i = 0; i < NUM_LEDS; i++) {
    const int o = i * 3;
    ctx.buffer[o] = r;
    ctx.buffer[o + 1] = g;
    ctx.buffer[o + 2] = b;
  }
}

}  // namespace

extern const Pattern kSolid = {"solid", nullptr, render};

}  // namespace cube
