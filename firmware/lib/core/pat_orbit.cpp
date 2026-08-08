// Port of src/shared/patterns/orbit.ts — atom-style orbiting particles.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_throb.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;
constexpr float kPi = 3.14159265358979f;
constexpr int MAX_PARTICLES = 8;
constexpr int TRAIL = 14;
// Orbiters fade in/out rather than popping when the active count changes.
constexpr float kFadeS = 0.35f;
// Smoothing for the level/bpm count sources, so a wobbling level doesn't
// flicker an orbiter on and off. (The beat source is already smoothed by the
// shared throb envelope.)
constexpr float kCountTau = 0.25f;

struct Orbiter {
  float ux, uy, uz;
  float vx, vy, vz;
  float radius;
  float phase;
  float freq;
  float alpha;  // 0..1 fade so orbs enter/leave smoothly
  float trail[TRAIL + 1][3];
  int trailLen;
};

float s_lastT = 0;
Orbiter s_orb[MAX_PARTICLES];
float s_spin = 0;
float s_countFrac = 0;  // smoothed audio drive for the active count
AudioThrob s_throb;

/**
 * Build the full pool once. Orientations depend only on the index, so the
 * active count can change every frame without disturbing the orbits that are
 * already flying.
 */
void makeOrbiters(float radius) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
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
    o.alpha = 0;
    o.trailLen = 0;
  }
}

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_spin = 0;
  s_countFrac = 0;
  s_throb.reset();
  makeOrbiters(3.4f);
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
  const char* countFrom = p.str("countFrom", "none");
  const float countGain = p.num("countGain", 1.5f);
  const int countMax = (int)std::fmax(1.0f, std::fmin((float)MAX_PARTICLES,
                                                      std::floor(p.num("countMax", 8.0f))));
  const float size = p.num("size", 0.5f);
  const float beatSize = p.num("beatSize", 0.5f);
  const char* paletteName = p.str("palette", "spectrum");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  for (int i = 0; i < MAX_PARTICLES; i++) s_orb[i].radius = radius;

  // Beat envelope drives both the size pulse and (optionally) the count; the
  // shared throb params decide how snappy it feels.
  const float beatEnv = s_throb.advance(*ctx.audio, dt, p.num("throbAttack", 0.03f),
                                        p.num("throbRelease", 0.5f));
  const float throb = 1.0f - clamp01(p.num("throbDepth", 0.0f)) * (1.0f - beatEnv);

  // How many orbiters are flying right now: `count` when quiet, up to
  // `countMax` as the music drives it.
  float target = 0;
  if (std::strcmp(countFrom, "level") == 0) {
    target = clamp01(ctx.audio->level * countGain);
  } else if (std::strcmp(countFrom, "bpm") == 0) {
    target = ctx.audio->bpm > 0 ? clamp01(countGain * ctx.audio->bpm / 240.0f) : 0.0f;
  } else if (std::strcmp(countFrom, "beat") == 0) {
    target = clamp01(beatEnv * countGain);
  }
  if (std::strcmp(countFrom, "beat") == 0 || kCountTau <= 0.005f) {
    s_countFrac = target;  // already smoothed by the throb envelope
  } else {
    s_countFrac += (target - s_countFrac) * (1.0f - std::exp(-dt / kCountTau));
  }
  // Colors spread across every orbiter this config can reach — but with no
  // audio source that's just `count`, so the palette spacing is unchanged.
  const bool audioCount = std::strcmp(countFrom, "none") != 0;
  const int ceilCount = audioCount && countMax > count ? countMax : count;
  const int active = count + (int)std::lround(s_countFrac * (ceilCount - count));

  const float c = (N - 1) / 2.0f;
  const float boost = 1.0f + ctx.audio->beat * beatSpeed;
  s_spin += precess * kPi * 2.0f * dt;
  const float cs = std::cos(s_spin), sn = std::sin(s_spin);
  const float headRadius = std::fmax(0.1f, size * (1.0f + beatSize * beatEnv));

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

  // Fatter-than-a-voxel particle: solid out to rad-0.5, linear fade to zero at
  // rad+0.5. At rad <= 0.5 this degenerates to the point splat above, so the
  // default size keeps the original look exactly.
  auto splatBall = [&](float px, float py, float pz, float rad, float r, float g,
                       float b) {
    if (rad <= 0.5f) { splat(px, py, pz, r, g, b); return; }
    const int lo[3] = {(int)std::floor(px - rad - 0.5f), (int)std::floor(py - rad - 0.5f),
                       (int)std::floor(pz - rad - 0.5f)};
    const int hi[3] = {(int)std::ceil(px + rad + 0.5f), (int)std::ceil(py + rad + 0.5f),
                       (int)std::ceil(pz + rad + 0.5f)};
    for (int zc = lo[2]; zc <= hi[2]; zc++) {
      if (zc < 0 || zc >= N) continue;
      const float dz = zc - pz;
      for (int yc = lo[1]; yc <= hi[1]; yc++) {
        if (yc < 0 || yc >= N) continue;
        const float dy = yc - py;
        for (int xc = lo[0]; xc <= hi[0]; xc++) {
          if (xc < 0 || xc >= N) continue;
          const float dx = xc - px;
          const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
          const float w = clamp01(rad + 0.5f - d);
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

  for (int oi = 0; oi < MAX_PARTICLES; oi++) {
    Orbiter& o = s_orb[oi];
    // Fade toward "flying" or "gone"; a faded-out orb keeps orbiting so it
    // rejoins the formation in phase when the music calls it back.
    const float aTarget = oi < active ? 1.0f : 0.0f;
    const float step = kFadeS > 0 ? dt / kFadeS : 1.0f;
    if (o.alpha < aTarget) o.alpha = std::fmin(aTarget, o.alpha + step);
    else if (o.alpha > aTarget) o.alpha = std::fmax(aTarget, o.alpha - step);

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

    if (o.alpha <= 0.01f) continue;

    float hr = 255, hg = 255, hb = 255;
    if (useP) {
      uint8_t rgb[3];
      // Colors span the palette across every orbiter this config can reach, so
      // an orb keeps its color as the audio-driven count moves around it.
      samplePalette(pal, (float)oi / std::fmax(1.0f, (float)ceilCount), t, rgb);
      hr = rgb[0]; hg = rgb[1]; hb = rgb[2];
    }
    const float bright = o.alpha * throb;
    for (int s = o.trailLen - 1; s >= 0; s--) {
      const float k = s == 0 ? headBright : headBright * (1.0f - (float)s / (trailLen + 1)) * 0.8f;
      if (k * bright <= 0.02f) continue;
      // Trail segments taper along with their brightness.
      const float rel = headBright > 0.01f ? clamp01(k / headBright) : 0.0f;
      const float rad = headRadius * (0.4f + 0.6f * rel);
      splatBall(o.trail[s][0], o.trail[s][1], o.trail[s][2], rad, hr * k * bright,
                hg * k * bright, hb * k * bright);
    }
  }
}

}  // namespace

extern const Pattern kOrbit = {"orbit", init, render};

}  // namespace cube
