// Port of src/shared/patterns/life3d.ts (Carter Bays-style 3D automaton).
#include <cmath>
#include <cstring>

#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

uint8_t s_grid[NUM_LEDS];
uint8_t s_next[NUM_LEDS];
uint16_t s_age[NUM_LEDS];
float s_lastStepT = 0;
int s_prevPopulation = -1;
int s_stagnationFrames = 0;

inline int indexAt(int x, int y, int z) {
  const int N = CUBE_N;
  return ((z + N) % N) * N * N + ((y + N) % N) * N + ((x + N) % N);
}

void reseed(float density) {
  for (int i = 0; i < NUM_LEDS; i++) {
    s_grid[i] = frand() < density ? 1 : 0;
    s_age[i] = s_grid[i] ? 1 : 0;
  }
}

// Parse "5-7/6" style S/B rules into neighbor-count masks (0..26).
void parseRule(const char* rule, bool survive[27], bool born[27]) {
  std::memset(survive, 0, 27);
  std::memset(born, 0, 27);
  bool* target = survive;
  int lo = -1, hi = -1;
  auto flush = [&]() {
    if (lo < 0) return;
    if (hi < 0) hi = lo;
    for (int n = lo; n <= hi && n < 27; n++) target[n] = true;
    lo = hi = -1;
  };
  bool inHi = false;
  for (const char* c = rule;; c++) {
    if (*c >= '0' && *c <= '9') {
      int& v = inHi ? hi : lo;
      v = (v < 0 ? 0 : v * 10) + (*c - '0');
    } else if (*c == '-') {
      inHi = true;
      hi = -1;
    } else {  // ',', '/', or end
      flush();
      inHi = false;
      if (*c == '/') target = born;
      if (*c == '\0') break;
    }
  }
}

void init(PatternCtx& ctx) {
  std::memset(s_grid, 0, sizeof(s_grid));
  std::memset(s_next, 0, sizeof(s_next));
  std::memset(s_age, 0, sizeof(s_age));
  reseed(ctx.params->num("density", 0.32f));
  s_lastStepT = ctx.t;
  s_prevPopulation = -1;
  s_stagnationFrames = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const int N = CUBE_N;

  bool survive[27], born[27];
  parseRule(p.str("rule", "5-7/6"), survive, born);
  const float stepHz = p.num("stepHz", 6.0f);
  const float stepInterval = 1.0f / std::fmax(0.5f, stepHz);
  const PaletteRef pal = resolvePalette(p.str("palette", "spectrum"));
  const float fadeAge = std::fmax(1.0f, p.num("fadeAge", 6.0f));

  if (t - s_lastStepT >= stepInterval) {
    s_lastStepT = t;
    int pop = 0;
    for (int z = 0; z < N; z++) {
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          // Moore-26 neighbour count with toroidal wrap.
          int c = 0;
          for (int dz = -1; dz <= 1; dz++) {
            for (int dy = -1; dy <= 1; dy++) {
              for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                c += s_grid[indexAt(x + dx, y + dy, z + dz)];
              }
            }
          }
          const int i = indexAt(x, y, z);
          const bool willLive = s_grid[i] ? survive[c] : born[c];
          s_next[i] = willLive ? 1 : 0;
          if (willLive) {
            s_age[i]++;
            pop++;
          } else {
            s_age[i] = 0;
          }
        }
      }
    }
    std::memcpy(s_grid, s_next, sizeof(s_grid));

    // Same population count for many ticks -> reseed.
    if (pop == s_prevPopulation) s_stagnationFrames++;
    else s_stagnationFrames = 0;
    s_prevPopulation = pop;
    if (pop == 0 || s_stagnationFrames > 60) {
      reseed(p.num("density", 0.32f));
      s_prevPopulation = -1;
      s_stagnationFrames = 0;
    }
  }

  // Render: alive cells colored by palette, hue by z layer, fade-in by age.
  for (int z = 0; z < N; z++) {
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        const int gi = indexAt(x, y, z);
        const int i = ctx.idx(x, y, z) * 3;
        if (!s_grid[gi]) {
          buffer[i] = buffer[i + 1] = buffer[i + 2] = 0;
          continue;
        }
        const float fade = std::fmin(1.0f, s_age[gi] / fadeAge);
        const float t01 = ((float)z / (N - 1)) * 0.7f + 0.3f * fade;
        uint8_t rgb[3];
        samplePalette(pal, t01, t, rgb);
        const float k = 0.4f + 0.6f * fade;
        buffer[i] = (uint8_t)std::lround(rgb[0] * k);
        buffer[i + 1] = (uint8_t)std::lround(rgb[1] * k);
        buffer[i + 2] = (uint8_t)std::lround(rgb[2] * k);
      }
    }
  }
}

}  // namespace

extern const Pattern kLife3d = {"life-3d", init, render};

}  // namespace cube
