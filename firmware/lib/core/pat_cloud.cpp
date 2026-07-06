// Port of src/shared/patterns/cloud.ts.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

float s_lastT = 0;
float s_lastBeat = 0;
float s_spin = 0;
float s_spinVel = 0;

// Cheap deterministic 3D noise: phase-offset sines at a few spatial
// frequencies so we get lumps within a 10-voxel cube.
float densityField(float x, float y, float z, float t) {
  return (std::sin(x * 1.7f + y * 0.9f + t * 0.6f) * 0.55f +
          std::sin(z * 1.3f - y * 1.1f + t * 0.4f) * 0.55f +
          std::sin((x + z) * 0.9f + y * 0.7f + t * 0.3f) * 0.45f +
          std::sin((x - z) * 1.1f + y * 1.4f - t * 0.35f) * 0.35f +
          std::sin(x * 2.6f + z * 2.2f + t * 0.5f) * 0.25f) /
         2.15f;
}

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_lastBeat = 0;
  s_spin = 0;
  s_spinVel = 0;
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
  const float threshold = p.num("threshold", 0.0f);
  const float scale = std::fmax(0.01f, p.num("scale", 0.5f));
  const float driftSpeed = p.num("driftSpeed", 0.25f);
  const float edge = std::fmax(0.02f, p.num("edge", 0.18f));
  const float baseR = p.num("r", 220.0f);
  const float baseG = p.num("g", 235.0f);
  const float baseB = p.num("b", 255.0f);
  const float topTint = clamp01(p.num("topTint", 0.4f));
  const float spinKick = p.num("spinKick", 1.4f);
  const float spinDamp = std::fmax(0.05f, p.num("spinDamp", 1.2f));
  const float beatThreshold = p.num("beatThreshold", 0.5f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  // Spin kick on rising edge of the beat envelope.
  const float beat = audio.beat;
  if (beat > beatThreshold && s_lastBeat <= beatThreshold) {
    const float dir = (s_spinVel >= 0 ? 1.0f : -1.0f) * (frand() < 0.85f ? 1.0f : -1.0f);
    s_spinVel += spinKick * dir * (0.7f + 0.6f * audio.level);
  }
  s_lastBeat = beat;
  s_spinVel *= std::exp(-spinDamp * dt);
  s_spin += s_spinVel * dt;

  std::memset(buffer, 0, NUM_LEDS * 3);

  const float c = (N - 1) / 2.0f;
  const float cosA = std::cos(s_spin);
  const float sinA = std::sin(s_spin);
  const float drift = t * driftSpeed;

  float dens[CUBE_N];

  for (int x = 0; x < N; x++) {
    for (int z = 0; z < N; z++) {
      // Rotate the (x,z) sample point around the vertical axis so the cloud
      // spins without remapping the LED grid.
      const float rx = (x - c) * cosA - (z - c) * sinA;
      const float rz = (x - c) * sinA + (z - c) * cosA;

      for (int y = 0; y < N; y++) {
        dens[y] = densityField(rx * scale, (y - c) * scale, rz * scale, drift);
      }

      for (int y = 0; y < N; y++) {
        const float d = dens[y];
        // Out-of-bounds counts as "still inside the cloud" so the cube's own
        // top/bottom face never reads as a cloud surface.
        const float dAbove = y < N - 1 ? dens[y + 1] : 1e3f;
        const float dBelow = y > 0 ? dens[y - 1] : 1e3f;

        const float here = clamp01((d - threshold) / edge);
        if (here <= 0) continue;
        const float topness = here * clamp01((threshold - dAbove) / edge);
        const float bottomness = here * clamp01((threshold - dBelow) / edge);
        float intensity = topness + bottomness;
        if (intensity <= 0.01f) continue;
        intensity = clamp01(intensity);

        float cr, cg, cb;
        if (useP) {
          uint8_t rgb[3];
          samplePalette(pal, (float)y / (N - 1), t, rgb);
          cr = rgb[0];
          cg = rgb[1];
          cb = rgb[2];
        } else {
          // Top of cloud (high y) keeps base; bottom darkens by topTint.
          const float shade = 1.0f - topTint * (1.0f - (float)y / (N - 1));
          cr = baseR * shade;
          cg = baseG * shade;
          cb = baseB * shade;
        }

        const int i = ctx.idx(x, y, z) * 3;
        buffer[i] = (uint8_t)std::lround(cr * intensity);
        buffer[i + 1] = (uint8_t)std::lround(cg * intensity);
        buffer[i + 2] = (uint8_t)std::lround(cb * intensity);
      }
    }
  }
}

}  // namespace

extern const Pattern kCloud = {"cloud", init, render};

}  // namespace cube
