// Port of src/shared/patterns/barEq.ts — 5x5 grid of 2x2-footprint columns
// throbbing with the spectrum, lows-to-highs corner-to-corner.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr int kBars = CUBE_N / 2;  // 5 per side, 2x2 cells each

float s_smoothed[kBars * kBars];

void init(PatternCtx& ctx) {
  std::memset(s_smoothed, 0, sizeof(s_smoothed));
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;
  const float dt = ctx.dt;
  const int N = CUBE_N;

  const float gain = p.num("gain", 1.5f);
  const float gamma = std::fmax(0.1f, p.num("gamma", 0.8f));
  const float attack = std::fmax(0.0f, p.num("attack", 0.03f));
  const float release = std::fmax(0.01f, p.num("release", 0.3f));
  const float beatBoost = clamp01(p.num("beatBoost", 0.2f));
  const float baseHeight = p.num("baseHeight", 0.6f);
  const float sat = p.num("sat", 0.9f);
  const char colorBy = p.str("colorBy", "bar")[0];  // 'b'ar | 'h'eight | ba'n'd
  const char colorBy2 = p.str("colorBy", "bar")[2];  // disambiguate bar/band
  const char* paletteName = p.str("palette", "spectrum");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  const float beat = audio.beat;

  std::memset(buffer, 0, NUM_LEDS * 3);

  for (int by = 0; by < kBars; by++) {
    for (int bx = 0; bx < kBars; bx++) {
      const int bi = by * kBars + bx;
      // Frequency position along the diagonal: (0,0) lows, (4,4) highs.
      const float bandPos = (float)(bx + by) / (2 * (kBars - 1));
      const float bf = bandPos * (AUDIO_BANDS - 1);
      const int b0 = (int)bf;
      const int b1 = b0 + 1 < AUDIO_BANDS ? b0 + 1 : AUDIO_BANDS - 1;
      const float f = bf - b0;
      const float target =
          clamp01((audio.bands[b0] * (1 - f) + audio.bands[b1] * f) * gain);
      const float tau = target > s_smoothed[bi] ? attack : release;
      const float k = tau <= 0 ? 1.0f : 1.0f - std::exp(-dt / tau);
      s_smoothed[bi] += (target - s_smoothed[bi]) * k;

      const float energy = clamp01(s_smoothed[bi] + beatBoost * beat);
      const float shaped = std::pow(energy, gamma);
      // Bar height in voxels; idle bars keep a dim base so the floor reads.
      const float height = baseHeight + shaped * (N - baseHeight);

      // 'bar' mode: one solid color per column, scrambled across the palette
      // so adjacent bars contrast instead of blending.
      const float barT = (float)((bi * 7) % (kBars * kBars)) / (kBars * kBars - 1);
      for (int z = 0; z < N; z++) {
        // Antialiased top: full below, fractional coverage at the crest.
        const float cover = clamp01(height - z);
        if (cover <= 0.02f) break;
        const float t01 = (colorBy == 'b' && colorBy2 == 'n') ? bandPos
                          : colorBy == 'b' ? barT
                                           : (float)z / (N - 1);
        uint8_t rgb[3];
        if (useP) {
          samplePalette(pal, t01, ctx.t, rgb);
        } else {
          // HSV fallback: green base -> yellow -> red crest, EQ-classic.
          hsvToRgb(0.33f - t01 * 0.33f, sat, 1.0f, rgb);
        }
        const uint8_t r = (uint8_t)std::lround(rgb[0] * cover);
        const uint8_t g = (uint8_t)std::lround(rgb[1] * cover);
        const uint8_t b = (uint8_t)std::lround(rgb[2] * cover);
        for (int ox = 0; ox <= 1; ox++) {
          for (int oy = 0; oy <= 1; oy++) {
            const int i = ctx.idx(bx * 2 + ox, by * 2 + oy, z) * 3;
            buffer[i] = r;
            buffer[i + 1] = g;
            buffer[i + 2] = b;
          }
        }
      }
    }
  }
}

}  // namespace

extern const Pattern kBarEq = {"bar-eq", init, render};

}  // namespace cube
