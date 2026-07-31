// Storm-cloud rework (firmware-original). The original port of cloud.ts
// rendered only the density field's top/bottom SURFACES — and treated y as
// vertical, so on the real cube its shading ran sideways and the result read
// as unstructured mush. v2 is built to be legible from across a room:
//  - a lumpy cloud mass hangs from the cube's CEILING (z up), drifting and
//    slowly spinning, undersides shaded darker like a real storm front;
//  - the air below stays black (sparse, high-contrast);
//  - beats — or an idle timer when there's no music — hurl zigzag lightning
//    bolts from the cloud's underside to the floor, and each strike briefly
//    lights the whole cloud from inside.
#include <algorithm>
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
float s_nextIdleBoltT = 0;

struct Bolt {
  int8_t x[CUBE_N], y[CUBE_N], z[CUBE_N];
  int len;
  float born;
};
constexpr int kMaxBolts = 4;
Bolt s_bolts[kMaxBolts];
int s_boltCount = 0;

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
  s_boltCount = 0;
  s_nextIdleBoltT = ctx.t + 1.0f;
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
  const float threshold = p.num("threshold", 0.1f);
  const float scale = std::fmax(0.01f, p.num("scale", 0.5f));
  const float driftSpeed = p.num("driftSpeed", 0.25f);
  const float edge = std::fmax(0.02f, p.num("edge", 0.18f));
  const float hang = clamp01(p.num("hang", 0.6f));
  const float baseR = p.num("r", 220.0f);
  const float baseG = p.num("g", 235.0f);
  const float baseB = p.num("b", 255.0f);
  const float topTint = clamp01(p.num("topTint", 0.5f));
  const float spinKick = p.num("spinKick", 0.8f);
  const float spinDamp = std::fmax(0.05f, p.num("spinDamp", 1.2f));
  const float beatThreshold = p.num("beatThreshold", 0.5f);
  const bool boltOnBeat = p.boolean("boltOnBeat", true);
  const float boltRate = p.num("boltRate", 0.15f);
  const float boltFade = std::fmax(0.05f, p.num("boltFade", 0.3f));
  const float flash = clamp01(p.num("flash", 0.35f));
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  // Beat rising edge: spin kick + bolt request. Idle timer keeps occasional
  // strikes coming when there's no music to react to.
  const float beat = audio.beat;
  bool wantBolt = false;
  if (beat > beatThreshold && s_lastBeat <= beatThreshold) {
    const float dir = (s_spinVel >= 0 ? 1.0f : -1.0f) * (frand() < 0.85f ? 1.0f : -1.0f);
    s_spinVel += spinKick * dir * (0.7f + 0.6f * audio.level);
    if (boltOnBeat) wantBolt = true;
  }
  s_lastBeat = beat;
  if (boltRate > 0 && t >= s_nextIdleBoltT) {
    wantBolt = true;
    s_nextIdleBoltT = t + (0.5f + frand()) / boltRate;
  }
  s_spinVel *= std::exp(-spinDamp * dt);
  s_spin += s_spinVel * dt;

  std::memset(buffer, 0, NUM_LEDS * 3);

  // Flash from bolts already in flight lights the cloud this frame (a bolt
  // spawned below reaches the cloud next frame — imperceptible at 30fps).
  float boltMax = 0;
  for (int bi = 0; bi < s_boltCount; bi++) {
    const float k = 1.0f - (t - s_bolts[bi].born) / boltFade;
    if (k > boltMax) boltMax = k;
  }
  const float cloudFlash = 1.0f + flash * 2.5f * clamp01(boltMax);

  const float c = (N - 1) / 2.0f;
  const float cosA = std::cos(s_spin);
  const float sinA = std::sin(s_spin);
  const float drift = t * driftSpeed;

  // Cloud pass. lowestLit[column] = underside z of the cloud (or -1).
  int8_t lowestLit[CUBE_N * CUBE_N];
  std::memset(lowestLit, -1, sizeof(lowestLit));
  for (int x = 0; x < N; x++) {
    for (int y = 0; y < N; y++) {
      // Spin the horizontal sample point around the vertical axis.
      const float rx = (x - c) * cosA - (y - c) * sinA;
      const float ry = (x - c) * sinA + (y - c) * cosA;
      for (int z = N - 1; z >= 0; z--) {
        const float z01 = (float)z / (N - 1);
        // Ceiling bias: density rises toward the top, sinks near the floor,
        // so the mass visibly hangs overhead with clear air below.
        const float bias = hang * (z01 * 1.4f - 0.7f);
        const float d =
            densityField(rx * scale, ry * scale, (z - c) * scale, drift) + bias;
        const float here = clamp01((d - threshold) / edge);
        if (here <= 0.02f) continue;
        lowestLit[x * N + y] = (int8_t)z;

        float cr, cg, cb;
        if (useP) {
          uint8_t rgb[3];
          samplePalette(pal, z01, t, rgb);
          cr = rgb[0];
          cg = rgb[1];
          cb = rgb[2];
        } else {
          // Storm shading: top of the cloud bright, underside darkened.
          const float shade = 1.0f - topTint * (1.0f - z01);
          cr = baseR * shade;
          cg = baseG * shade;
          cb = baseB * shade;
        }
        const float intensity = std::fmin(1.0f, here * cloudFlash);
        const int i = ctx.idx(x, y, z) * 3;
        buffer[i] = (uint8_t)std::lround(std::fmin(255.0f, cr * intensity));
        buffer[i + 1] = (uint8_t)std::lround(std::fmin(255.0f, cg * intensity));
        buffer[i + 2] = (uint8_t)std::lround(std::fmin(255.0f, cb * intensity));
      }
    }
  }

  // Spawn a bolt from a random column that has cloud with air beneath it.
  if (wantBolt && s_boltCount < kMaxBolts) {
    for (int attempt = 0; attempt < 6; attempt++) {
      const int bx = (int)(frand() * N);
      const int by = (int)(frand() * N);
      const int base = lowestLit[bx * N + by];
      if (base < 2) continue;  // no cloud here, or no room to strike
      Bolt& b = s_bolts[s_boltCount++];
      b.born = t;
      b.len = 0;
      int cx = bx, cy = by;
      for (int z = base; z >= 0; z--) {
        b.x[b.len] = (int8_t)cx;
        b.y[b.len] = (int8_t)cy;
        b.z[b.len] = (int8_t)z;
        b.len++;
        // Zigzag: occasional one-cell sidesteps on the way down.
        if (frand() < 0.35f) cx = std::max(0, std::min(N - 1, cx + (frand() < 0.5f ? -1 : 1)));
        if (frand() < 0.35f) cy = std::max(0, std::min(N - 1, cy + (frand() < 0.5f ? -1 : 1)));
      }
      break;
    }
  }

  // Render bolts: white-blue, snapping to full then fading out.
  int aliveBolts = 0;
  for (int bi = 0; bi < s_boltCount; bi++) {
    Bolt& b = s_bolts[bi];
    const float k = 1.0f - (t - b.born) / boltFade;
    if (k <= 0) continue;
    const float kk = k * k;  // fast falloff reads as a strike, not a glow
    for (int s = 0; s < b.len; s++) {
      const int i = ctx.idx(b.x[s], b.y[s], b.z[s]) * 3;
      const uint8_t vr = (uint8_t)std::lround(230.0f * kk);
      const uint8_t vg = (uint8_t)std::lround(240.0f * kk);
      const uint8_t vb = (uint8_t)std::lround(255.0f * kk);
      if (vr > buffer[i]) buffer[i] = vr;
      if (vg > buffer[i + 1]) buffer[i + 1] = vg;
      if (vb > buffer[i + 2]) buffer[i + 2] = vb;
    }
    s_bolts[aliveBolts++] = b;
  }
  s_boltCount = aliveBolts;
}

}  // namespace

extern const Pattern kCloud = {"cloud", init, render};

}  // namespace cube
