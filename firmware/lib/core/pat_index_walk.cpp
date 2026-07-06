// Port of src/shared/patterns/indexWalk.ts.
#include <cmath>
#include <cstring>

#include "cube_pattern.h"

namespace cube {
namespace {

void render(PatternCtx& ctx) {
  const float speed = ctx.params->num("speed", 25.0f);
  const int tail = (int)std::fmax(0.0f, std::floor(ctx.params->num("tail", 6.0f)));
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
  const int head = (int)(ctx.t * speed) % NUM_LEDS;
  for (int k = 0; k <= tail; k++) {
    const int i = (head - k + NUM_LEDS) % NUM_LEDS;
    const uint8_t v = (uint8_t)std::lround(255.0f * (1.0f - (float)k / (tail + 1)));
    const int o = i * 3;
    ctx.buffer[o] = v;
    ctx.buffer[o + 1] = v;
    ctx.buffer[o + 2] = v;
  }
}

}  // namespace

extern const Pattern kIndexWalk = {"index-walk", nullptr, render};

}  // namespace cube
