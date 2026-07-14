// Port of src/shared/patterns/orbit.ts — atom-style orbiting particles.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;
constexpr float kPi = 3.14159265358979f;
constexpr int MAX_PARTICLES = 6;
constexpr int TRAIL = 14;

struct Orbiter {
  float ux, uy, uz;
  float vx, vy, vz;
  float radius;
  float phase;
  float freq;
  float tCol;
  float trail[TRAIL + 1][3];
  int trailLen;
};

float s_lastT = 0;
Orbiter s_orb[MAX_PARTICLES];
int s_count = 0;
float s_spin = 0;

void makeOrbiters(int nn, float radius) {
  for (int i = 0; i < nn; i++) {
    Orbiter& o = s_orb[i];
    const float a = i * 2.399963f;  // golden angle
    const float b = i * 1.107f + 0.4f;
    const float nx = std::cos(a) * std::sin(b);
    const float ny = std::sin(a) * std::sin(b);
    const float nz = std::cos(b);
    float ux = -std::sin(a), uy = std::cos(a), uz = 0;
    const float ul = std::sqrt(ux * ux + uy * uy + uz * uz);
    ux /= ul; uy /= ul; uz /= ul;
    o.ux = ux; o.uy = uy; o.uz = uz;
    o.vx = ny * uz - nz * uy;
    o.vy = nz * ux - nx * uz;
    o.vz = nx * uy - ny * ux;
    o.radius = radius;
    o.phase = i * 1.7f;
    o.freq = 0.5f + 0.12f * i;
    o.tCol = (float)i / std::fmax(1, nn);
    o.trailLen = 0;
  }
  s_count = nn;
}

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_spin = 0;
  makeOrbiters(4, 3.4f);
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;
  std::memset(buffer, 0, NUM_LEDS * 3);

  const int count = (int)std::fmax(1.0f, std::fmin((float)MAX_PARTICLES, std::floor(p.num("count", 4.0f))));
  const float radius = p.num("radius", 3.4f);
  const float speed = p.num("speed", 0.4f);
  const int trailLen = (int)std::fmax(0.0f, std::fmin((float)TRAIL, std::floor(p.num("trail", 10.0f))));
  const float precess = p.num("precess", 0.05f);
  const float beatSpeed = p.num("beatSpeed", 1.5f);
  const float headBright = p.num("headBright", 1.0f);
  const char* paletteName = p.str("palette", "spectrum");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  if (s_count != count) makeOrbiters(count, radius);
  for (int i = 0; i < s_count; i++) s_orb[i].radius = radius;

  const float c = (N - 1) / 2.0f;
  const float boost = 1.0f + ctx.audio->beat * beatSpeed;
  s_spin += precess * kPi * 2.0f * dt;
  const float cs = std::cos(s_spin), sn = std::sin(s_spin);

  auto splat = [&](float px, float py, float pz, float r, float g, float b) {
    const int x0 = (int)std::floor(px), y0 = (int)std::floor(py), z0 = (int)std::floor(pz);
    const float fx = px - x0, fy = py - y0, fz = pz - z0;
    for (int dz = 0; dz <= 1; dz++) {
      const int zc = z0 + dz; if (zc < 0 || zc >= N) continue;
      const float wz = dz == 0 ? 1 - fz : fz;
      for (int dy = 0; dy <= 1; dy++) {
        const int yc = y0 + dy; if (yc < 0 || yc >= N) continue;
        const float wy = dy == 0 ? 1 - fy : fy;
        for (int dx = 0; dx <= 1; dx++) {
          const int xc = x0 + dx; if (xc < 0 || xc >= N) continue;
          const float w = (dx == 0 ? 1 - fx : fx) * wy * wz;
          if (w <= 0.02f) continue;
          const int i = ctx.idx(xc, yc, zc) * 3;
          const uint8_t rr = (uint8_t)std::lround(r * w);
          const uint8_t gg = (uint8_t)std::lround(g * w);
          const uint8_t bb = (uint8_t)std::lround(b * w);
          if (rr > buffer[i]) buffer[i] = rr;
          if (gg > buffer[i + 1]) buffer[i + 1] = gg;
          if (bb > buffer[i + 2]) buffer[i + 2] = bb;
        }
      }
    }
  };

  for (int oi = 0; oi < s_count; oi++) {
    Orbiter& o = s_orb[oi];
    o.phase += o.freq * speed * boost * kPi * 2.0f * dt;
    const float ca = std::cos(o.phase), sa = std::sin(o.phase);
    float px = o.radius * (ca * o.ux + sa * o.vx);
    float py = o.radius * (ca * o.uy + sa * o.vy);
    float pz = o.radius * (ca * o.uz + sa * o.vz);
    const float rx = px * cs + pz * sn;
    const float rz = -px * sn + pz * cs;
    px = c + rx; py = c + py; pz = c + rz;

    // Push onto the trail ring (head first), cap length.
    const int cap = trailLen + 1;
    for (int s = (o.trailLen < cap ? o.trailLen : cap - 1); s > 0; s--) {
      o.trail[s][0] = o.trail[s - 1][0];
      o.trail[s][1] = o.trail[s - 1][1];
      o.trail[s][2] = o.trail[s - 1][2];
    }
    o.trail[0][0] = px; o.trail[0][1] = py; o.trail[0][2] = pz;
    if (o.trailLen < cap) o.trailLen++;

    float hr = 255, hg = 255, hb = 255;
    if (useP) {
      uint8_t rgb[3];
      samplePalette(pal, o.tCol, t, rgb);
      hr = rgb[0]; hg = rgb[1]; hb = rgb[2];
    }
    for (int s = o.trailLen - 1; s >= 0; s--) {
      const float k = s == 0 ? headBright : headBright * (1.0f - (float)s / (trailLen + 1)) * 0.8f;
      if (k <= 0.02f) continue;
      splat(o.trail[s][0], o.trail[s][1], o.trail[s][2], hr * k, hg * k, hb * k);
    }
  }
}

}  // namespace

extern const Pattern kOrbit = {"orbit", init, render};

}  // namespace cube
