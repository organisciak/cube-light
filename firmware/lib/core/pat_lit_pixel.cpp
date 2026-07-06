// Port of src/shared/patterns/litPixel.ts (calibration helper).
#include <cmath>
#include <cstring>

#include "cube_pattern.h"

namespace cube {
namespace {

void render(PatternCtx& ctx) {
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
  const int idx = (int)std::fmax(
      0.0f, std::fmin((float)(NUM_LEDS - 1), std::floor(ctx.params->num("ledIdx", 0.0f))));
  const int o = idx * 3;
  ctx.buffer[o] = (uint8_t)ctx.params->num("r", 255.0f);
  ctx.buffer[o + 1] = (uint8_t)ctx.params->num("g", 255.0f);
  ctx.buffer[o + 2] = (uint8_t)ctx.params->num("b", 255.0f);
}

}  // namespace

extern const Pattern kLitPixel = {"lit-pixel", nullptr, render};

}  // namespace cube
