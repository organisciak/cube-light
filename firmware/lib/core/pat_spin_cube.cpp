// Port of src/shared/patterns/spinCube.ts — tumbling wireframe cube.
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
float s_angA = 0;
float s_angB = 0;
float s_boost = 0;

const int8_t CORNERS[8][3] = {
    {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
    {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
};
const uint8_t EDGES[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},  // bottom face
    {4, 5}, {5, 6}, {6, 7}, {7, 4},  // top face
    {0, 4}, {1, 5}, {2, 6}, {3, 7},  // verticals
};

void splatMax(PatternCtx& ctx, float px, float py, float pz, float r, float g, float b) {
  uint8_t* buffer = ctx.buffer;
  const int x0 = (int)std::floor(px), y0 = (int)std::floor(py), z0 = (int)std::floor(pz);
  const float fx = px - x0, fy = py - y0, fz = pz - z0;
  for (int dz = 0; dz <= 1; dz++) {
    const int cz = z0 + dz;
    if (cz < 0 || cz >= N) continue;
    const float wz = dz == 0 ? 1 - fz : fz;
    for (int dy = 0; dy <= 1; dy++) {
      const int cy = y0 + dy;
      if (cy < 0 || cy >= N) continue;
      const float wy = dy == 0 ? 1 - fy : fy;
      for (int dx = 0; dx <= 1; dx++) {
        const int cx = x0 + dx;
        if (cx < 0 || cx >= N) continue;
        const float w = (dx == 0 ? 1 - fx : fx) * wy * wz;
        if (w <= 0.02f) continue;
        const int i = ctx.idx(cx, cy, cz) * 3;
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
  s_angA = 0.5f;
  s_angB = 0.2f;
  s_boost = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);

  const float baseSize = p.num("size", 5.5f);
  const float speedA = p.num("speedA", 0.09f);
  const float speedB = p.num("speedB", 0.06f);
  const float breathe = p.num("breathe", 0.12f);
  const float beatKick = p.num("beatKick", 2.0f);
  const float cr = p.num("r", 240.0f);
  const float cg = p.num("g", 240.0f);
  const float cb = p.num("b", 255.0f);
  const float cornerBoost = p.num("cornerBoost", 1.35f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  const float beat = ctx.audio->beat;
  s_boost = std::fmax(s_boost * std::exp(-2.0f * dt), beat * beatKick);
  s_angA += speedA * (1.0f + s_boost) * kPi * 2.0f * dt;
  s_angB += speedB * (1.0f + s_boost * 0.6f) * kPi * 2.0f * dt;

  const float half = (baseSize * (1.0f + breathe * std::sin(t * 0.9f))) / 2.0f;
  const float c = (N - 1) / 2.0f;
  const float cosA = std::cos(s_angA), sinA = std::sin(s_angA);
  const float cosB = std::cos(s_angB), sinB = std::sin(s_angB);

  // Rotate corners: Y-axis by angB, then X-axis by angA.
  float pts[8][3];
  for (int i = 0; i < 8; i++) {
    const float x = CORNERS[i][0] * half;
    const float y = CORNERS[i][1] * half;
    const float z = CORNERS[i][2] * half;
    const float x1 = x * cosB + z * sinB;
    const float z1 = -x * sinB + z * cosB;
    const float y2 = y * cosA - z1 * sinA;
    const float z2 = y * sinA + z1 * cosA;
    pts[i][0] = c + x1;
    pts[i][1] = c + y2;
    pts[i][2] = c + z2;
  }

  for (int e = 0; e < 12; e++) {
    const float* a = pts[EDGES[e][0]];
    const float* b = pts[EDGES[e][1]];
    const float dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
    const int steps =
        (int)std::fmax(2.0f, std::ceil(std::sqrt(dx * dx + dy * dy + dz * dz) * 2.5f));
    float er = cr, eg = cg, eb = cb;
    if (useP) {
      // Each edge gets its own palette stop so the frame reads colorful.
      uint8_t rgb[3];
      samplePalette(pal, (float)e / 11.0f, t, rgb);
      er = rgb[0];
      eg = rgb[1];
      eb = rgb[2];
    }
    for (int s = 0; s <= steps; s++) {
      const float f = (float)s / steps;
      splatMax(ctx, a[0] + dx * f, a[1] + dy * f, a[2] + dz * f, er, eg, eb);
    }
  }
  // Corners a touch brighter — sells the vertices.
  for (int i = 0; i < 8; i++) {
    splatMax(ctx, pts[i][0], pts[i][1], pts[i][2], std::fmin(255.0f, cr * cornerBoost),
             std::fmin(255.0f, cg * cornerBoost), std::fmin(255.0f, cb * cornerBoost));
  }
}

}  // namespace

extern const Pattern kSpinCube = {"spin-cube", init, render};

}  // namespace cube
