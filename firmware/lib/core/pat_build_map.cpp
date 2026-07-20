// Port of src/shared/patterns/buildMap.ts — physical assembly helper.
// Lights LEDs by RAW WIRE INDEX (not geometry), so it works before the cube is
// calibrated. Marks each strand's start (offset 0, + a dim direction LED at
// offset 1) and its center (offsets 5/6) in a second color. Writes straight
// into the frame buffer, so both data pins (the 2x500 split) are covered.
#include <cmath>
#include <cstring>

#include "cube_pattern.h"

namespace cube {
namespace {

void render(PatternCtx& ctx) {
  uint8_t* buffer = ctx.buffer;
  std::memset(buffer, 0, NUM_LEDS * 3);
  const Params& p = *ctx.params;

  const int period = (int)std::fmax(2.0f, std::floor(p.num("period", 10.0f)));
  const uint8_t endR = (uint8_t)p.num("endR", 0.0f);
  const uint8_t endG = (uint8_t)p.num("endG", 255.0f);
  const uint8_t endB = (uint8_t)p.num("endB", 255.0f);
  const bool dimDir = p.boolean("dimDir", true);
  const bool showCenter = p.boolean("showCenter", true);
  const int centerOffset = (int)std::fmax(0.0f, std::floor(p.num("centerOffset", 5.0f)));
  const uint8_t ctrR = (uint8_t)p.num("ctrR", 255.0f);
  const uint8_t ctrG = (uint8_t)p.num("ctrG", 90.0f);
  const uint8_t ctrB = (uint8_t)p.num("ctrB", 0.0f);

  auto put = [&](int i, uint8_t r, uint8_t g, uint8_t b) {
    if (i < 0 || i >= NUM_LEDS) return;
    const int o = i * 3;
    buffer[o] = r;
    buffer[o + 1] = g;
    buffer[o + 2] = b;
  };

  for (int base = 0; base < NUM_LEDS; base += period) {
    put(base, endR, endG, endB);
    if (dimDir)
      put(base + 1, (uint8_t)std::lround(endR * 0.28f), (uint8_t)std::lround(endG * 0.28f),
          (uint8_t)std::lround(endB * 0.28f));
    if (showCenter && centerOffset < period) {
      put(base + centerOffset, ctrR, ctrG, ctrB);
      if (centerOffset + 1 < period) put(base + centerOffset + 1, ctrR, ctrG, ctrB);
    }
  }
}

}  // namespace

extern const Pattern kBuildMap = {"build-map", nullptr, render};

}  // namespace cube
