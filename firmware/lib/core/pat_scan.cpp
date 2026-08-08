// Port of src/shared/patterns/scan.ts — Cylon/CT scanner sweep.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;

float s_lastT = 0;
float s_pos = 0;
int s_dir = 1;
float s_hitPhase[N];  // ctx.t when each slice was last crossed

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_pos = 0;
  s_dir = 1;
  for (int i = 0; i < N; i++) s_hitPhase[i] = ctx.t - 99.0f;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;
  std::memset(buffer, 0, NUM_LEDS * 3);

  const char axis = p.str("axis", "z")[0];
  const float speed = p.num("speed", 11.0f);
  const float fade = std::fmax(0.05f, p.num("fade", 0.55f));
  const bool gridOnly = p.boolean("gridOnly", false);
  const bool colorBySlice = p.boolean("colorBySlice", true);
  const float sat = p.num("sat", 0.85f);
  const float levelGain = p.num("levelGain", 1.0f);
  const char* paletteName = p.str("palette", "arctic");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  const float prev = s_pos;
  s_pos += s_dir * speed * (1.0f + ctx.audio->level * levelGain) * dt;
  if (s_pos > N - 1) { s_pos = (N - 1) - (s_pos - (N - 1)); s_dir = -1; }
  else if (s_pos < 0) { s_pos = -s_pos; s_dir = 1; }
  const float from = std::fmin(prev, s_pos), to = std::fmax(prev, s_pos);
  for (int s = (int)std::ceil(from); s <= (int)std::floor(to); s++) {
    if (s >= 0 && s < N) s_hitPhase[s] = t;
  }

  const int mid = N / 2;
  for (int s = 0; s < N; s++) {
    const float age = t - s_hitPhase[s];
    if (age > fade) continue;
    const float env = clamp01(1.0f - age / fade);
    const float bright = env * env;
    const float tCol = colorBySlice ? (float)s / (N - 1) : 0.5f;
    uint8_t rgb[3];
    if (useP) samplePalette(pal, tCol, t, rgb);
    else hsvToRgb(0.55f - tCol * 0.4f, sat, 1.0f, rgb);
    const uint8_t cr = (uint8_t)std::lround(rgb[0] * bright);
    const uint8_t cg = (uint8_t)std::lround(rgb[1] * bright);
    const uint8_t cb = (uint8_t)std::lround(rgb[2] * bright);

    for (int a = 0; a < N; a++) {
      for (int b = 0; b < N; b++) {
        if (gridOnly) {
          const bool edge = a == 0 || a == N - 1 || b == 0 || b == N - 1;
          const bool cross = a == mid || b == mid;
          if (!edge && !cross) continue;
        }
        const int i =
            (axis == 'z' ? ctx.idx(a, b, s)
                         : axis == 'y' ? ctx.idx(a, s, b) : ctx.idx(s, a, b)) *
            3;
        buffer[i] = cr;
        buffer[i + 1] = cg;
        buffer[i + 2] = cb;
      }
    }
  }
}

}  // namespace

extern const Pattern kScan = {"scan", init, render};

}  // namespace cube
