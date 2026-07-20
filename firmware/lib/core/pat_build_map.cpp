// Port of src/shared/patterns/buildMap.ts — physical assembly helper.
//
// "axis" mode (geometry): light the two extreme planes and the two middle planes
//   on a chosen axis, via ctx.idx — so the lit faces follow the calibrated
//   layout/orientation and are the actual physical faces of the cube.
// "strand" mode (raw wire index, no calibration): mark each strand's two ENDS
//   (offset 0 and period-1, i.e. 0, 9,10, 19,20, …) plus the two middle LEDs.
//
// Both write straight into the frame buffer, covering both data pins (2x500 split).
#include <cmath>
#include <cstring>

#include "cube_pattern.h"

namespace cube {
namespace {

void render(PatternCtx& ctx) {
  uint8_t* buffer = ctx.buffer;
  std::memset(buffer, 0, NUM_LEDS * 3);
  const Params& p = *ctx.params;
  const int N = CUBE_N;

  const bool showCenter = p.boolean("showCenter", true);
  const uint8_t endR = (uint8_t)p.num("endR", 0.0f);
  const uint8_t endG = (uint8_t)p.num("endG", 255.0f);
  const uint8_t endB = (uint8_t)p.num("endB", 255.0f);
  const uint8_t ctrR = (uint8_t)p.num("ctrR", 255.0f);
  const uint8_t ctrG = (uint8_t)p.num("ctrG", 90.0f);
  const uint8_t ctrB = (uint8_t)p.num("ctrB", 0.0f);

  auto putRaw = [&](int i, uint8_t r, uint8_t g, uint8_t b) {
    if (i < 0 || i >= NUM_LEDS) return;
    const int o = i * 3;
    buffer[o] = r;
    buffer[o + 1] = g;
    buffer[o + 2] = b;
  };

  const char* mode = p.str("mode", "axis");
  if (std::strcmp(mode, "strand") == 0) {
    const int period = (int)std::fmax(2.0f, std::floor(p.num("period", 10.0f)));
    for (int i = 0; i < NUM_LEDS; i++) {
      const int off = i % period;
      if (off == 0 || off == period - 1) putRaw(i, endR, endG, endB);
    }
    if (showCenter) {
      const int midLo = (period - 1) / 2;
      const int midHi = midLo + 1;
      for (int base = 0; base < NUM_LEDS; base += period) {
        putRaw(base + midLo, ctrR, ctrG, ctrB);
        if (midHi < period) putRaw(base + midHi, ctrR, ctrG, ctrB);
      }
    }
    return;
  }

  // axis mode: light whole planes at a coordinate on the chosen axis.
  const char axis = p.str("axis", "x")[0];
  auto plane = [&](int c, uint8_t r, uint8_t g, uint8_t b) {
    for (int u = 0; u < N; u++) {
      for (int v = 0; v < N; v++) {
        const int i = (axis == 'x' ? ctx.idx(c, u, v)
                                   : axis == 'y' ? ctx.idx(u, c, v) : ctx.idx(u, v, c)) *
                      3;
        buffer[i] = r;
        buffer[i + 1] = g;
        buffer[i + 2] = b;
      }
    }
  };
  plane(0, endR, endG, endB);
  plane(N - 1, endR, endG, endB);
  if (showCenter) {
    const int midLo = (N - 1) / 2;
    plane(midLo, ctrR, ctrG, ctrB);
    plane(midLo + 1, ctrR, ctrG, ctrB);
  }
}

}  // namespace

extern const Pattern kBuildMap = {"build-map", nullptr, render};

}  // namespace cube
