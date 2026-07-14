// Port of src/shared/patterns/bounce.ts — wireframe sphere bouncing + tumbling.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;
constexpr float kPi = 3.14159265358979f;

float s_lastT = 0;
float s_cx, s_cy, s_cz;
float s_vx, s_vy, s_vz;
float s_spin = 0;
float s_pulse = 0;

void splatMax(PatternCtx& ctx, float px, float py, float pz, float r, float g, float b) {
  uint8_t* buffer = ctx.buffer;
  const int x0 = (int)std::floor(px), y0 = (int)std::floor(py), z0 = (int)std::floor(pz);
  const float fx = px - x0, fy = py - y0, fz = pz - z0;
  for (int dz = 0; dz <= 1; dz++) {
    const int zc = z0 + dz;
    if (zc < 0 || zc >= N) continue;
    const float wz = dz == 0 ? 1 - fz : fz;
    for (int dy = 0; dy <= 1; dy++) {
      const int yc = y0 + dy;
      if (yc < 0 || yc >= N) continue;
      const float wy = dy == 0 ? 1 - fy : fy;
      for (int dx = 0; dx <= 1; dx++) {
        const int xc = x0 + dx;
        if (xc < 0 || xc >= N) continue;
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
}

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_cx = N * 0.4f; s_cy = N * 0.55f; s_cz = N * 0.45f;
  s_vx = 0.7f; s_vy = 1.0f; s_vz = 0.85f;
  s_spin = 0; s_pulse = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);

  const float baseR = p.num("radius", 2.6f);
  const float speed = p.num("speed", 5.0f);
  const int nRings = (int)std::fmax(1.0f, std::floor(p.num("rings", 3.0f)));
  const int nMer = (int)std::fmax(0.0f, std::floor(p.num("meridians", 4.0f)));
  const float spinSpeed = p.num("spinSpeed", 0.15f);
  const float beatPulse = p.num("beatPulse", 0.5f);
  const float cr = p.num("r", 120.0f), cg = p.num("g", 220.0f), cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "ocean");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  s_pulse = std::fmax(s_pulse * std::exp(-4.0f * dt), ctx.audio->beat * beatPulse);
  const float R = baseR + s_pulse;

  float vlen = std::sqrt(s_vx * s_vx + s_vy * s_vy + s_vz * s_vz);
  if (vlen == 0) vlen = 1;
  s_cx += (s_vx / vlen) * speed * dt;
  s_cy += (s_vy / vlen) * speed * dt;
  s_cz += (s_vz / vlen) * speed * dt;
  const float lo = R, hi = N - 1 - R;
  if (s_cx < lo) { s_cx = lo + (lo - s_cx); s_vx = std::fabs(s_vx); }
  else if (s_cx > hi) { s_cx = hi - (s_cx - hi); s_vx = -std::fabs(s_vx); }
  if (s_cy < lo) { s_cy = lo + (lo - s_cy); s_vy = std::fabs(s_vy); }
  else if (s_cy > hi) { s_cy = hi - (s_cy - hi); s_vy = -std::fabs(s_vy); }
  if (s_cz < lo) { s_cz = lo + (lo - s_cz); s_vz = std::fabs(s_vz); }
  else if (s_cz > hi) { s_cz = hi - (s_cz - hi); s_vz = -std::fabs(s_vz); }

  s_spin += spinSpeed * kPi * 2.0f * dt;
  const float cs = std::cos(s_spin), sn = std::sin(s_spin);

  auto emit = [&](float bx, float by, float bz, float tCol) {
    const float rx = bx * cs + bz * sn;
    const float rz = -bx * sn + bz * cs;
    float er = cr, eg = cg, eb = cb;
    if (useP) {
      uint8_t rgb[3];
      samplePalette(pal, tCol, t, rgb);
      er = rgb[0]; eg = rgb[1]; eb = rgb[2];
    }
    splatMax(ctx, s_cx + rx, s_cy + by, s_cz + rz, er, eg, eb);
  };

  const int K = (int)std::fmax(10.0f, std::lround(R * 6.0f));

  for (int li = 0; li < nRings; li++) {
    const float theta = ((li + 1.0f) / (nRings + 1)) * kPi;
    const float ringR = R * std::sin(theta);
    const float y = R * std::cos(theta);
    const float tCol = (float)li / std::fmax(1, nRings - 1);
    for (int k = 0; k < K; k++) {
      const float phi = ((float)k / K) * kPi * 2.0f;
      emit(ringR * std::cos(phi), y, ringR * std::sin(phi), tCol);
    }
  }
  for (int mi = 0; mi < nMer; mi++) {
    const float phi = ((float)mi / nMer) * kPi * 2.0f;
    const float cph = std::cos(phi), sph = std::sin(phi);
    const float tCol = (float)mi / std::fmax(1, nMer);
    for (int k = 0; k <= K; k++) {
      const float theta = ((float)k / K) * kPi;
      const float rr = R * std::sin(theta);
      emit(rr * cph, R * std::cos(theta), rr * sph, tCol);
    }
  }
}

}  // namespace

extern const Pattern kBounce = {"bounce", init, render};

}  // namespace cube
