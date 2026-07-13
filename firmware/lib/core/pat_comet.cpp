// Port of src/shared/patterns/comet.ts. Particle list becomes a ring-ish
// fixed pool (1 spawn/frame, tailLife <= 5s -> ~150 max; pool holds 192).
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

struct CometParticle {
  float x, y, z;
  float age;
};

constexpr int kMaxParticles = 192;
CometParticle s_particles[kMaxParticles];
int s_particleCount = 0;
float s_lastT = 0;
float s_px, s_py, s_pz;
float s_vx, s_vy, s_vz;

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_particleCount = 0;
  // Start near a corner with a non-axis-aligned heading so the comet visits
  // the interior rather than ping-ponging on a single face.
  s_px = 1.5f;
  s_py = 2.3f;
  s_pz = 0.7f;
  s_vx = 0.78f;
  s_vy = 1.15f;
  s_vz = 0.93f;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;

  const int N = CUBE_N;
  const float baseSpeed = p.num("speed", 8.0f);
  const float tailLife = std::fmax(0.05f, p.num("tailLife", 1.4f));
  const float tailGamma = std::fmax(0.1f, p.num("tailGamma", 1.6f));
  const float headR = p.num("r", 255.0f);
  const float headG = p.num("g", 255.0f);
  const float headB = p.num("b", 255.0f);
  const float beatGain = p.num("beatGain", 2.5f);
  const float beatThreshold = p.num("beatThreshold", 0.25f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  // Beat envelope above the threshold maps to a smooth speed boost that
  // decays back as the envelope falls.
  const float beat = ctx.audio->beat;
  const float beatExtra =
      beat > beatThreshold
          ? (beat - beatThreshold) / std::fmax(0.001f, 1.0f - beatThreshold)
          : 0.0f;
  const float speed = baseSpeed * (1.0f + beatExtra * beatGain);

  float vLen = std::sqrt(s_vx * s_vx + s_vy * s_vy + s_vz * s_vz);
  if (vLen == 0) vLen = 1;
  s_px += (s_vx / vLen) * speed * dt;
  s_py += (s_vy / vLen) * speed * dt;
  s_pz += (s_vz / vLen) * speed * dt;

  // Reflect off cube walls, DVD-logo style.
  const float lo = 0;
  const float hi = (float)(N - 1);
  if (s_px < lo) { s_px = lo + (lo - s_px); s_vx = std::fabs(s_vx); }
  else if (s_px > hi) { s_px = hi - (s_px - hi); s_vx = -std::fabs(s_vx); }
  if (s_py < lo) { s_py = lo + (lo - s_py); s_vy = std::fabs(s_vy); }
  else if (s_py > hi) { s_py = hi - (s_py - hi); s_vy = -std::fabs(s_vy); }
  if (s_pz < lo) { s_pz = lo + (lo - s_pz); s_vz = std::fabs(s_vz); }
  else if (s_pz > hi) { s_pz = hi - (s_pz - hi); s_vz = -std::fabs(s_vz); }

  // Spawn at the new head, expire stale, compact in place (order preserved:
  // oldest first, so the head still renders last).
  if (s_particleCount < kMaxParticles) {
    s_particles[s_particleCount++] = {s_px, s_py, s_pz, 0.0f};
  }
  int alive = 0;
  for (int i = 0; i < s_particleCount; i++) {
    s_particles[i].age += dt;
    if (s_particles[i].age <= tailLife) s_particles[alive++] = s_particles[i];
  }
  s_particleCount = alive;

  std::memset(buffer, 0, NUM_LEDS * 3);

  for (int pi = 0; pi < s_particleCount; pi++) {
    const CometParticle& pt = s_particles[pi];
    const float lifeFrac = clamp01(1.0f - pt.age / tailLife);
    const float intensity = std::pow(lifeFrac, tailGamma);
    if (intensity <= 0.001f) continue;

    float cr, cg, cb;
    if (useP) {
      // Palette pos 0 = head (newest), 1 = oldest.
      uint8_t rgb[3];
      samplePalette(pal, 1.0f - lifeFrac, t, rgb);
      cr = rgb[0];
      cg = rgb[1];
      cb = rgb[2];
    } else {
      cr = headR;
      cg = headG;
      cb = headB;
    }

    // Trilinear splat to the 8 surrounding voxels for smooth sub-cell motion.
    const int x0 = (int)std::floor(pt.x);
    const int y0 = (int)std::floor(pt.y);
    const int z0 = (int)std::floor(pt.z);
    const float fx = pt.x - x0;
    const float fy = pt.y - y0;
    const float fz = pt.z - z0;
    for (int ox = 0; ox <= 1; ox++) {
      const int cx = x0 + ox;
      if (cx < 0 || cx >= N) continue;
      const float wx = ox ? fx : 1.0f - fx;
      for (int oy = 0; oy <= 1; oy++) {
        const int cy = y0 + oy;
        if (cy < 0 || cy >= N) continue;
        const float wy = oy ? fy : 1.0f - fy;
        for (int oz = 0; oz <= 1; oz++) {
          const int cz = z0 + oz;
          if (cz < 0 || cz >= N) continue;
          const float wz = oz ? fz : 1.0f - fz;
          const float w = wx * wy * wz * intensity;
          if (w <= 0) continue;
          const int i = ctx.idx(cx, cy, cz) * 3;
          const uint8_t rr = (uint8_t)std::lround(cr * w);
          const uint8_t gg = (uint8_t)std::lround(cg * w);
          const uint8_t bb = (uint8_t)std::lround(cb * w);
          if (rr > buffer[i]) buffer[i] = rr;
          if (gg > buffer[i + 1]) buffer[i + 1] = gg;
          if (bb > buffer[i + 2]) buffer[i + 2] = bb;
        }
      }
    }
  }
}

}  // namespace

extern const Pattern kComet = {"comet", init, render};

}  // namespace cube
