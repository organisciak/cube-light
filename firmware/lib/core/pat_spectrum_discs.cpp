// Port of src/shared/patterns/spectrumDiscs.ts.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

float s_smoothed[CUBE_N];

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
  const char axis = p.str("axis", "y")[0];
  const float minRadius = std::fmax(0.0f, p.num("minRadius", 1.0f));
  const float gain = p.num("gain", 1.5f);
  const float gamma = std::fmax(0.1f, p.num("gamma", 0.7f));
  const float edge = std::fmax(0.1f, p.num("edge", 0.7f));
  const float attack = std::fmax(0.0f, p.num("attack", 0.04f));
  const float release = std::fmax(0.01f, p.num("release", 0.25f));
  const float sat = p.num("sat", 0.85f);
  const float floorAmt = clamp01(p.num("floor", 0.1f));
  const float beatBoost = clamp01(p.num("beatBoost", 0.25f));
  const char* paletteName = p.str("palette", "spectrum");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  const float center = (N - 1) / 2.0f;
  const float cornerR = std::sqrt(center * center * 2.0f);
  const float maxRadius = cornerR + 0.5f;
  const float beat = audio.beat;

  // Per-slice target energies: map slice 0..N-1 onto band axis with linear
  // interpolation, then asymmetric attack/release smoothing.
  for (int i = 0; i < N; i++) {
    const float bf = ((float)i / (N - 1)) * (AUDIO_BANDS - 1);
    const int b0 = (int)bf;
    const int b1 = b0 + 1 < AUDIO_BANDS ? b0 + 1 : AUDIO_BANDS - 1;
    const float f = bf - b0;
    const float target = clamp01((audio.bands[b0] * (1 - f) + audio.bands[b1] * f) * gain);
    const float tau = target > s_smoothed[i] ? attack : release;
    const float k = tau <= 0 ? 1.0f : 1.0f - std::exp(-dt / tau);
    s_smoothed[i] += (target - s_smoothed[i]) * k;
  }

  std::memset(buffer, 0, NUM_LEDS * 3);

  for (int s = 0; s < N; s++) {
    const float energy = clamp01(s_smoothed[s] + beatBoost * beat);
    const float shaped = std::pow(energy, gamma);
    const float radius = minRadius + (maxRadius - minRadius) * shaped;

    const float pos = (float)s / (N - 1);
    uint8_t rgb[3];
    if (useP) {
      samplePalette(pal, pos, ctx.t, rgb);
    } else {
      hsvToRgb(pos, sat, 1.0f, rgb);
    }

    // Small floor so quiet slices still glow faintly.
    const float brightness = clamp01(floorAmt + (1.0f - floorAmt) * shaped);

    for (int a = 0; a < N; a++) {
      for (int b = 0; b < N; b++) {
        const float dx = a - center;
        const float dy = b - center;
        const float d = std::sqrt(dx * dx + dy * dy);
        // Antialiased disc: 1 inside, fades over `edge` voxels at the rim.
        const float cover = clamp01((radius - d) / edge + 0.5f);
        if (cover <= 0) continue;
        const float v = cover * brightness;
        const int i =
            (axis == 'x' ? ctx.idx(s, a, b)
                         : axis == 'y' ? ctx.idx(a, s, b) : ctx.idx(a, b, s)) *
            3;
        buffer[i] = (uint8_t)std::lround(rgb[0] * v);
        buffer[i + 1] = (uint8_t)std::lround(rgb[1] * v);
        buffer[i + 2] = (uint8_t)std::lround(rgb[2] * v);
      }
    }
  }
}

}  // namespace

extern const Pattern kSpectrumDiscs = {"spectrum-discs", init, render};

}  // namespace cube
