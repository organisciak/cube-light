// Port of src/shared/patterns/fire.ts (3D Fire2012-style).
#include <cmath>
#include <cstring>

#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

float s_heat[NUM_LEDS];
float s_lastT = 0;

inline int H(int x, int y, int z) { return (z * CUBE_N + y) * CUBE_N + x; }

void init(PatternCtx& ctx) {
  std::memset(s_heat, 0, sizeof(s_heat));
  s_lastT = ctx.t;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;

  const int N = CUBE_N;
  const float cooling = p.num("cooling", 1.4f);
  const float sparking = p.num("sparking", 0.55f);
  const float sparkHeat = p.num("sparkHeat", 200.0f);
  // Clamped both ends: an out-of-range value (bad mod config / raw API call)
  // would index s_heat past the cube.
  const int baseLayers = (int)std::fmax(
      1.0f, std::fmin((float)N, std::floor(p.num("baseLayers", 2.0f))));
  const float drift = std::fmin(1.0f, p.num("driftRate", 0.85f));
  const PaletteRef pal = resolvePalette(p.str("palette", "fire"));
  const float audioGain = p.num("audioGain", 2.0f);

  // Effective frame step: scaled to a 30fps reference so behaviour is
  // fps-stable.
  const float fStep = std::fmin(2.0f, dt * 30.0f);

  // 1) Cool every cell.
  for (int i = 0; i < NUM_LEDS; i++) {
    s_heat[i] = std::fmax(0.0f, s_heat[i] - (frand() * cooling + 0.4f) * fStep);
  }

  // 2) Heat drifts upward: top-down, each cell blends toward the average of
  //    the two below (weighted 2:1 toward the nearer one).
  for (int z = N - 1; z >= 2; z--) {
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        const float a = s_heat[H(x, y, z - 1)];
        const float b = s_heat[H(x, y, z - 2)];
        const float c = s_heat[H(x, y, z)];
        s_heat[H(x, y, z)] = c * (1.0f - drift) + (a + b + a) / 3.0f * drift;
      }
    }
  }

  // 3) Sparks at the base, fanned by audio level + beat.
  const float audioBoost = 1.0f + ctx.audio->level * audioGain + ctx.audio->beat * 1.5f;
  for (int y = 0; y < N; y++) {
    for (int x = 0; x < N; x++) {
      if (frand() < sparking * audioBoost * fStep * 0.6f) {
        const int z = (int)(frand() * baseLayers);
        const float add = sparkHeat * (0.6f + frand() * 0.4f);
        s_heat[H(x, y, z)] = std::fmin(255.0f, s_heat[H(x, y, z)] + add);
      }
    }
  }

  // 4) Render heat -> color via palette.
  for (int z = 0; z < N; z++) {
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        const float t01 = std::fmax(0.0f, std::fmin(1.0f, s_heat[H(x, y, z)] / 255.0f));
        uint8_t rgb[3];
        samplePalette(pal, t01, t, rgb);
        const int i = ctx.idx(x, y, z) * 3;
        buffer[i] = rgb[0];
        buffer[i + 1] = rgb[1];
        buffer[i + 2] = rgb[2];
      }
    }
  }
}

}  // namespace

extern const Pattern kFire = {"fire", init, render};

}  // namespace cube
