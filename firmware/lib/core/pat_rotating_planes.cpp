// Port of src/shared/patterns/rotatingPlanes.ts.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr float kPi = 3.14159265358979f;

// Phase accumulators in radians — angular speed varies with audio per frame,
// so phase can't be derived as t * speed.
float s_lastT = 0;
float s_rotPhase = 0;
float s_sweepPhase = 0;

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_rotPhase = 0;
  s_sweepPhase = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;

  const int N = CUBE_N;
  const float baseSpeed = p.num("speed", 0.3f);
  const float baseSweepSpeed = p.num("sweepSpeed", 0.4f);
  const float baseThickness = std::fmax(0.1f, p.num("thickness", 1.0f));
  const float audioGain = p.num("audioGain", 1.5f);
  const float levelSpeedGain = p.num("levelSpeedGain", 1.5f);
  const float beatSpeedGain = p.num("beatSpeedGain", 2.0f);
  const float levelSweepGain = p.num("levelSweepGain", 1.2f);
  const float beatSweepGain = p.num("beatSweepGain", 1.5f);
  const float level = clamp01(audio.level);
  const float beat = audio.beat;
  const float thickness = baseThickness * (1.0f + level * audioGain);
  const int planeCount = (int)std::fmax(1.0f, std::floor(p.num("planes", 2.0f)));
  const float cr = p.num("r", 80.0f);
  const float cg = p.num("g", 200.0f);
  const float cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);
  const float c = (N - 1) / 2.0f;

  const float rotRate =
      baseSpeed * (1.0f + level * levelSpeedGain) + baseSpeed * beat * beatSpeedGain;
  const float sweepRate =
      baseSweepSpeed * (1.0f + level * levelSweepGain) + baseSweepSpeed * beat * beatSweepGain;
  s_rotPhase += rotRate * kPi * 2.0f * dt;
  s_sweepPhase += sweepRate * kPi * 0.7f * dt;

  std::memset(buffer, 0, NUM_LEDS * 3);

  for (int pi = 0; pi < planeCount; pi++) {
    const float phase = s_rotPhase + (pi * kPi) / planeCount;
    // Normal vector rotates in the xy-plane and tilts in z over time.
    const float nx = std::cos(phase);
    const float ny = std::sin(phase);
    const float nz = std::sin(phase * 0.6f + pi * 0.3f);
    float nLen = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nLen == 0) nLen = 1;
    const float ux = nx / nLen;
    const float uy = ny / nLen;
    const float uz = nz / nLen;
    const float off = std::sin(s_sweepPhase + (pi * kPi) / planeCount) * 3.5f;

    float pr = cr, pg = cg, pb = cb;
    if (useP) {
      uint8_t rgb[3];
      samplePalette(pal, (pi + 0.5f) / planeCount, t, rgb);
      pr = rgb[0];
      pg = rgb[1];
      pb = rgb[2];
    }

    for (int z = 0; z < N; z++) {
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          const float d = std::fabs(ux * (x - c) + uy * (y - c) + uz * (z - c) - off);
          if (d > thickness) continue;
          const float fall = 1.0f - d / thickness;
          const int i = ctx.idx(x, y, z) * 3;
          const uint8_t rr = (uint8_t)std::lround(pr * fall);
          const uint8_t gg = (uint8_t)std::lround(pg * fall);
          const uint8_t bb = (uint8_t)std::lround(pb * fall);
          if (rr > buffer[i]) buffer[i] = rr;
          if (gg > buffer[i + 1]) buffer[i + 1] = gg;
          if (bb > buffer[i + 2]) buffer[i + 2] = bb;
        }
      }
    }
  }
}

}  // namespace

extern const Pattern kRotatingPlanes = {"rotating-planes", init, render};

}  // namespace cube
