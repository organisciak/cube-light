// Rez tunnel (firmware-original) — flying forward through a synesthetic
// tunnel, the cube as your viewport: depth runs along the chosen axis
// (default +y, or a slow "spin" pan). Each beat launches
// a wireframe shape (ring / square / X) from the far horizon that grows in
// perspective as it flies past; ambient sparks stream by continuously, and
// the forward speed surges with the music. Silence = a sparse drifting
// starfield; a beat drop = shapes stacking up and rushing in.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

constexpr int kMaxP = 144;
constexpr float kMaxDepth = 14.0f;  // world spawn distance; view shows 0..9
constexpr float kPersp = 4.0f;      // proj = P/(depth+P)

struct Particle {
  float wx, wz;  // world lateral offset from the tunnel axis
  float d;       // distance ahead; hits the viewer at 0
  uint8_t r, g, b;
  bool on;
};

Particle s_p[kMaxP];
float s_lastBeat;
float s_ambAcc;
bool s_seeded;

void init(PatternCtx& ctx) {
  for (int i = 0; i < kMaxP; i++) s_p[i].on = false;
  s_lastBeat = 0;
  s_ambAcc = 0;
  s_seeded = false;  // first render pre-fills the starfield (needs params)
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

// Free slot, or steal the particle closest to expiring so bursts always land.
int slot() {
  int best = 0;
  float bestD = 1e9f;
  for (int i = 0; i < kMaxP; i++) {
    if (!s_p[i].on) return i;
    if (s_p[i].d < bestD) {
      bestD = s_p[i].d;
      best = i;
    }
  }
  return best;
}

void spawn(float wx, float wz, float d, uint8_t r, uint8_t g, uint8_t b) {
  s_p[slot()] = {wx, wz, d, r, g, b, true};
}

// One wireframe shape at the far horizon, all points sharing one color.
// kind: 0 ring, 1 square outline, 2 X cross.
void spawnShape(int kind, float size, uint8_t r, uint8_t g, uint8_t b) {
  const float rot = frand() * 6.2831853f;
  if (kind == 0) {
    for (int i = 0; i < 12; i++) {
      const float a = rot + i * 6.2831853f / 12;
      spawn(std::cos(a) * size, std::sin(a) * size, kMaxDepth, r, g, b);
    }
  } else if (kind == 1) {
    const float h = size * 0.8f;
    for (int i = 0; i < 12; i++) {
      // Perimeter parametrization: 3 points per side.
      const int side = i / 3;
      const float f = (i % 3) / 3.0f * 2 - 1 + 1.0f / 3;  // -2/3, 0, 2/3
      float px = 0, pz = 0;
      if (side == 0) { px = f * h; pz = h; }
      else if (side == 1) { px = h; pz = -f * h; }
      else if (side == 2) { px = -f * h; pz = -h; }
      else { px = -h; pz = f * h; }
      spawn(px, pz, kMaxDepth, r, g, b);
    }
  } else {
    for (int j = 1; j <= 3; j++) {
      const float e = size * j / 3.0f;
      spawn(e, e, kMaxDepth, r, g, b);
      spawn(-e, -e, kMaxDepth, r, g, b);
      spawn(e, -e, kMaxDepth, r, g, b);
      spawn(-e, e, kMaxDepth, r, g, b);
    }
    spawn(0, 0, kMaxDepth, r, g, b);
  }
}

inline void splatMax(PatternCtx& ctx, uint8_t* buf, int x, int y, int z,
                     float r, float g, float b) {
  if (x < 0 || x >= CUBE_N || y < 0 || y >= CUBE_N || z < 0 || z >= CUBE_N)
    return;
  uint8_t* px = buf + ctx.idx(x, y, z) * 3;
  const uint8_t rr = (uint8_t)std::lround(r);
  const uint8_t gg = (uint8_t)std::lround(g);
  const uint8_t bb = (uint8_t)std::lround(b);
  if (rr > px[0]) px[0] = rr;
  if (gg > px[1]) px[1] = gg;
  if (bb > px[2]) px[2] = bb;
}

// Orthonormal view frame: which cube direction the tunnel runs along.
// "spin" rotates the frame around the design vertical over time, so the
// tunnel mouth slowly wheels around the cube.
struct Basis {
  float fx, fy, fz;  // forward (depth)
  float rx, ry, rz;  // right (lateral)
  float ux, uy, uz;  // up (vertical)
};

Basis viewBasis(const char* axis, float t, float spinDegPerSec) {
  if (axis[0] == 's') {
    const float th = t * spinDegPerSec * 0.0174533f;
    const float c = std::cos(th), s = std::sin(th);
    return {c, s, 0, -s, c, 0, 0, 0, 1};
  }
  if (axis[0] == 'x') return {1, 0, 0, 0, 1, 0, 0, 0, 1};
  if (axis[0] == 'z') return {0, 0, 1, 1, 0, 0, 0, 1, 0};
  return {0, 1, 0, 1, 0, 0, 0, 0, 1};  // y — the original viewport
}

// Splat a view-space point (depth u ahead, l right of center, v above
// center) through the basis into cube voxels; depth spans the cube center.
inline void splatView(PatternCtx& ctx, uint8_t* buf, const Basis& B, float u,
                      float l, float v, float r, float g, float b) {
  const float du = u - 4.5f;
  const int x = (int)std::lround(4.5f + B.fx * du + B.rx * l + B.ux * v);
  const int y = (int)std::lround(4.5f + B.fy * du + B.ry * l + B.uy * v);
  const int z = (int)std::lround(4.5f + B.fz * du + B.rz * l + B.uz * v);
  splatMax(ctx, buf, x, y, z, r, g, b);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;

  const float speed = p.num("speed", 6.0f);
  const char* speedFrom = p.str("speedFrom", "level");
  const float speedGain = p.num("speedGain", 1.5f);
  const float beatKick = p.num("beatKick", 1.2f);
  const int beatSpawn = (int)p.num("beatSpawn", 1.0f);
  const char* shapeName = p.str("shape", "mixed");
  const float shapeSize = p.num("shapeSize", 4.5f);
  const float ambientRate = p.num("ambientRate", 4.0f);
  const float streak = clamp01(p.num("streak", 0.4f));
  const float cr = p.num("r", 255.0f);
  const float cg = p.num("g", 255.0f);
  const float cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "rainbow");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);
  const Basis B = viewBasis(p.str("axis", "y"), ctx.t, p.num("spinSpeed", 8.0f));

  // One color per spawned thing: a random palette position, or the RGB knobs.
  const auto pickColor = [&](uint8_t out[3], float dim) {
    if (useP) {
      samplePalette(pal, frand(), ctx.t, out);
    } else {
      out[0] = (uint8_t)cr;
      out[1] = (uint8_t)cg;
      out[2] = (uint8_t)cb;
    }
    for (int c = 0; c < 3; c++) out[c] = (uint8_t)(out[c] * dim);
  };

  // Seed the starfield across the whole depth range on the first frame, so
  // a pattern switch doesn't open on a dark cube while sparks fly in.
  if (!s_seeded) {
    s_seeded = true;
    for (int i = 0; i < 24; i++) {
      const float ang = frand() * 6.2831853f;
      const float rad = 2.5f + frand() * 4.0f;
      uint8_t col[3];
      pickColor(col, 0.5f);
      spawn(std::cos(ang) * rad, std::sin(ang) * rad, frand() * kMaxDepth,
            col[0], col[1], col[2]);
    }
  }

  // Beat rising edge: launch shapes from the horizon.
  const float beat = audio.beat;
  if (beat > 0.5f && s_lastBeat <= 0.5f) {
    for (int k = 0; k < beatSpawn; k++) {
      int kind = shapeName[0] == 'r' ? 0 : shapeName[0] == 's' ? 1 :
                 shapeName[0] == 'c' ? 2 : (int)(frand() * 3);
      uint8_t col[3];
      pickColor(col, 1.0f);
      spawnShape(kind, shapeSize * (0.75f + 0.5f * frand()), col[0], col[1], col[2]);
    }
  }
  s_lastBeat = beat;

  const float dt = std::fmax(0.0f, std::fmin(0.1f, ctx.dt));

  // Ambient sparks drift in regardless of music, faster streams when loud.
  s_ambAcc += ambientRate * (1.0f + audio.level) * dt;
  while (s_ambAcc >= 1.0f) {
    s_ambAcc -= 1.0f;
    const float ang = frand() * 6.2831853f;
    const float rad = 2.5f + frand() * 4.0f;
    uint8_t col[3];
    pickColor(col, 0.5f);
    spawn(std::cos(ang) * rad, std::sin(ang) * rad, kMaxDepth * (0.7f + 0.3f * frand()),
          col[0], col[1], col[2]);
  }

  const float v = speed * audioSpeedMult(audio, speedFrom, speedGain) *
                  (1.0f + beatKick * beat);

  std::memset(buffer, 0, NUM_LEDS * 3);

  for (int i = 0; i < kMaxP; i++) {
    if (!s_p[i].on) continue;
    s_p[i].d -= v * dt;
    if (s_p[i].d < -0.5f) {
      s_p[i].on = false;
      continue;
    }
    const float d = std::fmax(0.0f, s_p[i].d);
    if (d > (float)CUBE_N - 0.6f) continue;  // still beyond the far face
    const float proj = kPersp / (d + kPersp);
    const float l = s_p[i].wx * proj;
    const float v2 = s_p[i].wz * proj;
    // Fade in from the horizon, brighten on approach.
    const float bright = clamp01((kMaxDepth - s_p[i].d) / 4.0f) *
                         (0.45f + 0.55f * proj);
    splatView(ctx, buffer, B, d, l, v2, s_p[i].r * bright, s_p[i].g * bright,
              s_p[i].b * bright);
    if (streak > 0)
      splatView(ctx, buffer, B, d + 1, l, v2, s_p[i].r * bright * streak,
                s_p[i].g * bright * streak, s_p[i].b * bright * streak);
  }
}

}  // namespace

extern const Pattern kRezTunnel = {"rez-tunnel", init, render};

}  // namespace cube
