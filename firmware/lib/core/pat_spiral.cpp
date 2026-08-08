// Port of src/shared/patterns/spiral.ts — square spirals with a full empty
// cell between arms, drawn and unwound per layer with phase delay + twist.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_throb.h"

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
float s_offSpeed = -1;  // low-passed speed for the per-layer offset (<0 = unset)
AudioThrob s_throb;

// "cycle" axis mode: each layer's effective axis follows its OWN pass number
// (z -> y -> x -> z ...), computed per-layer in render(). Because a layer is
// empty at its own pass boundary, layers can sit on different axes during a
// transition — a trailing spiral finishes on the old axis while a new one
// begins on the next axis — with no visible jump.
const char kAxisCycle[3] = {'z', 'y', 'x'};

void init(PatternCtx& ctx) {
  buildSpiralPath();
  s_lastT = ctx.t;
  s_phase = 0;
  s_offSpeed = -1;
  s_throb.reset();
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
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  // Beat-synced brightness throb (shared module; same knobs as snake/text).
  const float throb = s_throb.update(ctx);

  const int len = s_pathLen;
  const float cycle = 2.0f * len;
  s_phase += speed * (1.0f + ctx.audio->level * levelGain) * dt;
  // Wrap to keep float precision over multi-day runs. 3*cycle preserves the
  // 3-axis pass parity of "cycle" mode; wrapping only above 6*cycle keeps
  // s_phase well above the largest per-layer offset so raw never re-enters
  // the not-yet-started (<0) region.
  if (s_phase > 6.0f * cycle) s_phase -= 3.0f * cycle;

  // The per-layer offset scales with speed; a discrete speed change (slider,
  // param mod) would teleport every layer's phase — mid-arm axis flips in
  // cycle mode. Low-pass the speed used for the offset so changes glide.
  if (s_offSpeed < 0) s_offSpeed = speed;
  s_offSpeed += (speed - s_offSpeed) * std::fmin(1.0f, 2.0f * dt);

  for (int h = 0; h < N; h++) {
    const float raw = s_phase - h * layerDelay * s_offSpeed;
    if (raw < 0) continue;  // this layer's first pass hasn't started yet
    const float pph = std::fmod(std::fmod(raw, cycle) + cycle, cycle);
    const bool drawing = pph < len;
    const int from = drawing ? 0 : (int)(pph - len);
    const int to = drawing ? (int)pph : len;
    if (to <= from) continue;

    // Per-layer axis for "cycle" mode: each layer follows the axis of ITS OWN
    // pass, so a trailing layer finishes its spiral on the old axis while
    // leading layers begin the next pass on the next axis. A layer is empty at
    // its own pass boundary, so this per-layer switch is seamless.
    const char layerAxis =
        axis == 'c' ? kAxisCycle[(((int)std::floor(raw / cycle)) % 3 + 3) % 3]
                    : axis;

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
      const float k = (1.0f - age * (1.0f - tailDim)) * throb;
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
