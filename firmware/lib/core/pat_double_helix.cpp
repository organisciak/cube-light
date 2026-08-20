// Double helix (firmware-original) — DNA for the cube: two (or 1-3) point
// strands winding around a chosen axis, with optional base-pair rungs
// bridging them. Sparse by design: thin strands, dark interior. The whole
// structure rotates; beats can kick the spin and puff the radius, and the
// rotation rate can follow loudness or tempo like the other patterns.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

constexpr float kTau = 6.2831853f;
constexpr int kSub = 5;  // strand samples per layer (keeps high twist joined)

float s_phase;

void init(PatternCtx& ctx) {
  s_phase = frand() * kTau;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

inline void splatMax(PatternCtx& ctx, uint8_t* buf, int x, int y, int z,
                     float r, float g, float b) {
  if (x < 0 || x >= CUBE_N || y < 0 || y >= CUBE_N || z < 0 || z >= CUBE_N)
    return;
  uint8_t* px = buf + ctx.idx(x, y, z) * 3;
  const uint8_t rr = (uint8_t)std::lround(r);
  const uint8_t gg = (uint8_t)std::lround(g);
  const uint8_t bb = (uint8_t)std::lround(b);
  if (rr > px[0]) px[0] = rr;
  if (gg > px[1]) px[1] = gg;
  if (bb > px[2]) px[2] = bb;
}

// (a, b) spin in the plane perpendicular to the axis; h runs along it.
inline void splatAxis(PatternCtx& ctx, uint8_t* buf, char axis, float a,
                      float b, float h, float r, float g, float bl) {
  const int ia = (int)std::lround(a);
  const int ib = (int)std::lround(b);
  const int ih = (int)std::lround(h);
  if (axis == 'y') splatMax(ctx, buf, ia, ih, ib, r, g, bl);
  else if (axis == 'x') splatMax(ctx, buf, ih, ia, ib, r, g, bl);
  else splatMax(ctx, buf, ia, ib, ih, r, g, bl);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;

  const char axis = p.str("axis", "z")[0];
  const float radius = p.num("radius", 3.0f);
  const float turns = p.num("turns", 1.2f);
  const float speed = p.num("speed", 0.25f);
  const char* speedFrom = p.str("speedFrom", "none");
  const float speedGain = p.num("speedGain", 1.5f);
  const float beatSpin = p.num("beatSpin", 3.0f);
  const float beatPulse = p.num("beatPulse", 0.8f);
  const int strands = (int)std::fmax(1.0f, std::fmin(3.0f, p.num("strands", 2.0f)));
  const int rungEvery = (int)p.num("rungEvery", 3.0f);
  const float rungBright = clamp01(p.num("rungBright", 0.4f));
  const float cr = p.num("r", 80.0f);
  const float cg = p.num("g", 200.0f);
  const float cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "cycle");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  const float dt = std::fmax(0.0f, std::fmin(0.1f, ctx.dt));
  const float beat = audio.beat;
  s_phase += (speed * kTau * audioSpeedMult(audio, speedFrom, speedGain) +
              beatSpin * beat) * dt;
  const float rE = radius + beatPulse * beat;
  const float twist = kTau * turns / (CUBE_N - 1);  // radians per layer

  std::memset(buffer, 0, NUM_LEDS * 3);

  const auto strandColor = [&](int k, float h, uint8_t out[3]) {
    if (useP) {
      // Each strand offset along the palette so they read as distinct.
      float pos = h / (CUBE_N - 1) + (float)k / strands;
      pos -= std::floor(pos);
      samplePalette(pal, pos, ctx.t, out);
    } else {
      out[0] = (uint8_t)cr;
      out[1] = (uint8_t)cg;
      out[2] = (uint8_t)cb;
    }
  };

  constexpr int kSteps = (CUBE_N - 1) * kSub + 1;
  for (int i = 0; i < kSteps; i++) {
    const float h = (float)i / kSub;      // 0..9 along the axis
    const float th = s_phase + h * twist;
    for (int k = 0; k < strands; k++) {
      const float tk = th + kTau * k / strands;
      uint8_t col[3];
      strandColor(k, h, col);
      splatAxis(ctx, buffer, axis, 4.5f + rE * std::cos(tk),
                4.5f + rE * std::sin(tk), h, col[0], col[1], col[2]);
    }
  }

  // Base-pair rungs: straight bridges between the two strands every
  // rungEvery layers (they pass through the axis, so this reads as DNA).
  // Only drawn for the classic 2-strand form.
  if (strands == 2 && rungEvery > 0 && rungBright > 0) {
    for (int L = 0; L < CUBE_N; L += rungEvery) {
      const float h = (float)L;
      const float th = s_phase + h * twist;
      const float ax = 4.5f + rE * std::cos(th), ay = 4.5f + rE * std::sin(th);
      const float bx = 4.5f - rE * std::cos(th), by = 4.5f - rE * std::sin(th);
      uint8_t col[3];
      strandColor(0, h, col);
      const int n = (int)std::ceil(rE * 2) + 1;
      for (int j = 1; j < n; j++) {  // endpoints already lit by the strands
        const float f = (float)j / n;
        splatAxis(ctx, buffer, axis, ax + (bx - ax) * f, ay + (by - ay) * f, h,
                  col[0] * rungBright, col[1] * rungBright, col[2] * rungBright);
      }
    }
  }
}

}  // namespace

extern const Pattern kDoubleHelix = {"double-helix", init, render};

}  // namespace cube
