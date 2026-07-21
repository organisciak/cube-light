// Port of src/shared/patterns/spiral.ts — square spirals with a full empty
// cell between arms, drawn and unwound per layer with phase delay + twist.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;

struct PathCell {
  int8_t x, y;
};
PathCell s_path[N * N];
int s_pathLen = 0;

void buildSpiralPath() {
  if (s_pathLen > 0) return;
  int lo = 0, hi = N - 1;
  bool first = true;
  while (lo <= hi) {
    for (int x = first ? lo : lo - 1; x <= hi; x++)
      s_path[s_pathLen++] = {(int8_t)x, (int8_t)lo};
    for (int y = lo + 1; y <= hi; y++) s_path[s_pathLen++] = {(int8_t)hi, (int8_t)y};
    if (hi > lo) {
      for (int x = hi - 1; x >= lo; x--) s_path[s_pathLen++] = {(int8_t)x, (int8_t)hi};
      for (int y = hi - 1; y >= lo + 2; y--) s_path[s_pathLen++] = {(int8_t)lo, (int8_t)y};
    }
    first = false;
    lo += 2;
    hi -= 2;
  }
}

float s_lastT = 0;
float s_phase = 0;

// "cycle" axis mode: the effective layer axis advances z -> y -> x -> z each
// time the spiral completes a full pass (draw + unwind) through the cube.
const char kAxisCycle[3] = {'z', 'y', 'x'};
int s_axisIndex = 0;
int s_lastPass = 0;

void init(PatternCtx& ctx) {
  buildSpiralPath();
  s_lastT = ctx.t;
  s_phase = 0;
  s_axisIndex = 0;
  s_lastPass = 0;
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
  const float speed = p.num("speed", 18.0f);
  const float layerDelay = p.num("layerDelay", 0.14f);
  const int twist = (int)std::fmax(0.0f, std::floor(p.num("twist", 1.0f)));
  const float tailDim = clamp01(p.num("tailDim", 0.45f));
  const float levelGain = p.num("levelGain", 1.0f);
  const float sat = p.num("sat", 0.9f);
  const bool colorByLayer = p.str("colorBy", "position")[0] == 'l';
  const char* paletteName = p.str("palette", "cyberpunk");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  const int len = s_pathLen;
  const float cycle = 2.0f * len;
  s_phase += speed * (1.0f + ctx.audio->level * levelGain) * dt;

  // Detect full-pass boundaries: s_phase advances by `cycle` for one complete
  // draw+unwind. When axis == "cycle", advance the effective layer axis each
  // time a new pass completes.
  const int pass = (int)std::floor(s_phase / cycle);
  if (axis == 'c' && pass > s_lastPass) {
    s_axisIndex = (s_axisIndex + (pass - s_lastPass)) % 3;
  }
  s_lastPass = pass;
  const char layerAxis = axis == 'c' ? kAxisCycle[s_axisIndex] : axis;

  for (int h = 0; h < N; h++) {
    const float raw = s_phase - h * layerDelay * speed;
    const float pph = std::fmod(std::fmod(raw, cycle) + cycle, cycle);
    const bool drawing = pph < len;
    const int from = drawing ? 0 : (int)(pph - len);
    const int to = drawing ? (int)pph : len;
    if (to <= from) continue;

    const int turns = ((twist * h) / N) % 4;

    for (int i = from; i < to; i++) {
      int cx = s_path[i].x;
      int cy = s_path[i].y;
      for (int r = 0; r < turns; r++) {
        const int nx = cy;
        const int ny = N - 1 - cx;
        cx = nx;
        cy = ny;
      }
      const float age = (float)(to - 1 - i) / std::fmax(1, to - from);
      const float k = 1.0f - age * (1.0f - tailDim);
      const float t01 = colorByLayer ? (float)h / (N - 1) : (float)i / (len - 1);
      uint8_t rgb[3];
      if (useP) samplePalette(pal, t01, t, rgb);
      else hsvToRgb(t01 * 0.8f, sat, 1.0f, rgb);
      const int o =
          (layerAxis == 'z' ? ctx.idx(cx, cy, h)
                            : layerAxis == 'y' ? ctx.idx(cx, h, cy)
                                               : ctx.idx(h, cx, cy)) *
          3;
      const uint8_t rr = (uint8_t)std::lround(rgb[0] * k);
      const uint8_t gg = (uint8_t)std::lround(rgb[1] * k);
      const uint8_t bb = (uint8_t)std::lround(rgb[2] * k);
      if (rr > buffer[o]) buffer[o] = rr;
      if (gg > buffer[o + 1]) buffer[o + 1] = gg;
      if (bb > buffer[o + 2]) buffer[o + 2] = bb;
    }
  }
}

}  // namespace

extern const Pattern kSpiral = {"spiral", init, render};

}  // namespace cube
