// Port of src/shared/patterns/plasma.ts.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr float kPi = 3.14159265358979f;

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const int N = CUBE_N;
  const float speed = p.num("speed", 0.6f);
  const float scale = std::fmax(0.5f, p.num("scale", 6.0f));
  const float hueShift = p.num("hueShift", 0.0f);
  const float hueRange = p.num("hueRange", 0.6f);
  const float sat = clamp01(p.num("sat", 1.0f));
  const float baseBright = clamp01(p.num("bright", 0.6f));
  const float audioGain = p.num("audioGain", 0.6f);
  const float bright = clamp01(baseBright * (1.0f + ctx.audio->level * audioGain));
  const char* paletteName = p.str("palette", "rainbow");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);
  const float k = (kPi * 2.0f) / scale;
  const float wt = ctx.t * speed;

  for (int z = 0; z < N; z++) {
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        const float v =
            (std::sin(k * x + wt * 1.0f) + std::sin(k * y + wt * 1.3f) +
             std::sin(k * z + wt * 0.7f) +
             std::sin(k * (x + y + z) * 0.33f + wt * 1.1f)) /
                8.0f +
            0.5f;
        const float pos = hueShift + v * hueRange + wt * 0.05f;
        uint8_t rgb[3];
        if (useP) {
          samplePalette(pal, pos - std::floor(pos), ctx.t, rgb);
          rgb[0] = (uint8_t)std::lround(rgb[0] * bright);
          rgb[1] = (uint8_t)std::lround(rgb[1] * bright);
          rgb[2] = (uint8_t)std::lround(rgb[2] * bright);
        } else {
          hsvToRgb(pos, sat, bright, rgb);
        }
        const int i = ctx.idx(x, y, z) * 3;
        ctx.buffer[i] = rgb[0];
        ctx.buffer[i + 1] = rgb[1];
        ctx.buffer[i + 2] = rgb[2];
      }
    }
  }
}

}  // namespace

extern const Pattern kPlasma = {"plasma", nullptr, render};

}  // namespace cube
