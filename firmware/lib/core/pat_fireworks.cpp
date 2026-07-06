// Port of src/shared/patterns/fireworks.ts. Rocket/particle vectors become
// fixed pools: 8 rockets, 768 particles (default burst is 70 particles with
// ~1.8s max life and a 1.6s launch interval -> ~300 live typical).
#include <cmath>
#include <cstring>

#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

struct Vec3 {
  float x, y, z;
};

constexpr int kTrailLen = 8;

struct Rocket {
  Vec3 pos;
  Vec3 vel;
  float explodeAt;
  Vec3 trail[kTrailLen];  // head first
  int trailLen;
};

struct Particle {
  Vec3 pos;
  Vec3 vel;
  uint8_t color[3];
  float born;
  float life;
};

constexpr int kMaxRockets = 8;
constexpr int kMaxParticles = 768;

Rocket s_rockets[kMaxRockets];
int s_rocketCount = 0;
Particle s_particles[kMaxParticles];
int s_particleCount = 0;
float s_nextLaunchT = 0;
float s_lastBeatLaunchT = -10;

// Random launch point on the outside of the cube (biased toward the -z face
// so most rockets fly up), aimed at a jittered point near the center.
void randomLaunch(Vec3& pos, Vec3& target) {
  const int N = CUBE_N;
  const int face = (int)(frand() * 6);
  const float a = frand() * (N - 1);
  const float b = frand() * (N - 1);
  const float lo = -1.5f;         // ~1.5 cells outside the face
  const float hi = N - 1 - lo;    // mirrored on the far side
  switch (face) {
    case 0: pos = {lo, a, b}; break;
    case 1: pos = {hi, a, b}; break;
    case 2: pos = {a, lo, b}; break;
    case 3: pos = {a, hi, b}; break;
    case 4: pos = {a, b, lo}; break;
    default: pos = {a, b, hi}; break;
  }
  if (frand() < 0.5f) pos = {a, b, lo};

  const float c = (N - 1) / 2.0f;
  const float jitter = (N - 1) * 0.18f;
  target = {c + (frand() * 2 - 1) * jitter, c + (frand() * 2 - 1) * jitter,
            c + (frand() * 2 - 1) * jitter};
}

void launch(float t, float flightTime) {
  if (s_rocketCount >= kMaxRockets) return;
  Vec3 pos, target;
  randomLaunch(pos, target);
  Rocket& r = s_rockets[s_rocketCount++];
  r.pos = pos;
  r.vel = {(target.x - pos.x) / flightTime, (target.y - pos.y) / flightTime,
           (target.z - pos.z) / flightTime};
  r.explodeAt = t + flightTime;
  r.trailLen = 0;
}

// Burst: pick two palette stops, ~30% accent, per-particle brightness and
// speed jitter, directions uniform on the sphere (Marsaglia).
void spawnBurst(const Vec3& center, int count, float speed, const PaletteRef& pal,
                float t, float life) {
  uint8_t baseColor[3], accentColor[3];
  const float baseT = frand();
  const float accentT = std::fmod(baseT + 0.2f + frand() * 0.4f, 1.0f);
  samplePalette(pal, baseT, t, baseColor);
  samplePalette(pal, accentT, t, accentColor);

  for (int i = 0; i < count && s_particleCount < kMaxParticles; i++) {
    float u, v, s;
    do {
      u = frand() * 2 - 1;
      v = frand() * 2 - 1;
      s = u * u + v * v;
    } while (s >= 1 || s == 0);
    const float f = 2.0f * std::sqrt(1.0f - s);
    const float dx = u * f;
    const float dy = v * f;
    const float dz = 1.0f - 2.0f * s;

    const float sp = speed * (0.55f + frand() * 0.9f);
    const uint8_t* c = frand() < 0.3f ? accentColor : baseColor;
    const float k = 0.75f + frand() * 0.5f;

    Particle& p = s_particles[s_particleCount++];
    p.pos = center;
    p.vel = {dx * sp, dy * sp, dz * sp};
    p.color[0] = (uint8_t)std::fmin(255.0f, std::lround(c[0] * k));
    p.color[1] = (uint8_t)std::fmin(255.0f, std::lround(c[1] * k));
    p.color[2] = (uint8_t)std::fmin(255.0f, std::lround(c[2] * k));
    p.born = t;
    p.life = life * (0.7f + frand() * 0.6f);
  }
}

// Trilinear additive splat; out-of-bounds corners clipped so partial halos
// still show as particles leave the cube.
void splat(PatternCtx& ctx, float px, float py, float pz, float r, float g, float b) {
  const int N = CUBE_N;
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
        const float wx = dx == 0 ? 1 - fx : fx;
        const float w = wx * wy * wz;
        if (w <= 0.001f) continue;
        const int i = ctx.idx(cx, cy, cz) * 3;
        buffer[i] = (uint8_t)std::fmin(255.0f, buffer[i] + std::lround(r * w));
        buffer[i + 1] = (uint8_t)std::fmin(255.0f, buffer[i + 1] + std::lround(g * w));
        buffer[i + 2] = (uint8_t)std::fmin(255.0f, buffer[i + 2] + std::lround(b * w));
      }
    }
  }
}

void init(PatternCtx& ctx) {
  s_rocketCount = 0;
  s_particleCount = 0;
  s_nextLaunchT = ctx.t + 0.3f;
  s_lastBeatLaunchT = -10;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = ctx.dt;
  const int N = CUBE_N;

  const PaletteRef pal = resolvePalette(p.str("palette", "cyberpunk"));
  const float launchInterval = std::fmax(0.1f, p.num("launchInterval", 1.6f));
  const float flightTime = std::fmax(0.1f, p.num("flightTime", 0.7f));
  const int baseParticles = (int)std::fmax(1.0f, std::floor(p.num("particles", 70.0f)));
  const float burstSpeed = p.num("burstSpeed", 5.5f);
  const float gravity = p.num("gravity", 2.5f);
  const float drag = std::fmax(0.0f, p.num("drag", 1.4f));
  const float lifetime = std::fmax(0.1f, p.num("lifetime", 1.4f));
  const float rocketB = p.num("rocketBrightness", 0.95f) < 0
                            ? 0.0f
                            : std::fmin(1.0f, p.num("rocketBrightness", 0.95f));
  const bool beatLaunch = p.boolean("beatLaunch", true);
  const float levelBoost = p.num("levelBoost", 0.6f);

  const float beat = audio.beat;
  const float level = audio.level;

  // Schedule launches.
  if (t >= s_nextLaunchT) {
    launch(t, flightTime);
    s_nextLaunchT = t + launchInterval;
  }
  if (beatLaunch && beat > 0.6f && t - s_lastBeatLaunchT > 0.18f) {
    launch(t, flightTime * (0.7f + frand() * 0.4f));
    s_lastBeatLaunchT = t;
  }

  // Step rockets; explode into particles at explodeAt.
  int aliveRockets = 0;
  for (int ri = 0; ri < s_rocketCount; ri++) {
    Rocket& r = s_rockets[ri];
    r.pos.x += r.vel.x * dt;
    r.pos.y += r.vel.y * dt;
    r.pos.z += r.vel.z * dt;
    if (r.trailLen < kTrailLen) r.trailLen++;
    for (int k = r.trailLen - 1; k > 0; k--) r.trail[k] = r.trail[k - 1];
    r.trail[0] = r.pos;

    if (t >= r.explodeAt) {
      const int burstScale = (int)std::lround(baseParticles * (1.0f + level * levelBoost));
      spawnBurst(r.pos, burstScale, burstSpeed, pal, t, lifetime);
    } else {
      s_rockets[aliveRockets++] = r;
    }
  }
  s_rocketCount = aliveRockets;

  // Step particles; drop dead / far-out-of-bounds ones.
  const float dragK = std::exp(-drag * dt);
  int aliveParticles = 0;
  for (int pi = 0; pi < s_particleCount; pi++) {
    Particle& pt = s_particles[pi];
    pt.vel.x *= dragK;
    pt.vel.y *= dragK;
    pt.vel.z = pt.vel.z * dragK - gravity * dt;
    pt.pos.x += pt.vel.x * dt;
    pt.pos.y += pt.vel.y * dt;
    pt.pos.z += pt.vel.z * dt;
    const float age = t - pt.born;
    if (age >= pt.life) continue;
    if (pt.pos.x < -3 || pt.pos.x > N + 2 || pt.pos.y < -3 || pt.pos.y > N + 2 ||
        pt.pos.z < -3 || pt.pos.z > N + 2)
      continue;
    s_particles[aliveParticles++] = pt;
  }
  s_particleCount = aliveParticles;

  // ----- render -----
  std::memset(buffer, 0, NUM_LEDS * 3);

  // Rockets: bright white head + short falloff trail.
  for (int ri = 0; ri < s_rocketCount; ri++) {
    const Rocket& r = s_rockets[ri];
    for (int i = 0; i < r.trailLen; i++) {
      const Vec3& seg = r.trail[i];
      const float k = (1.0f - (float)i / r.trailLen) * rocketB;
      const float v = 255.0f * k;
      splat(ctx, seg.x, seg.y, seg.z, v, v, v);
    }
  }

  // Particles: quick rise to peak at ~10% life, then slow fall.
  for (int pi = 0; pi < s_particleCount; pi++) {
    const Particle& pt = s_particles[pi];
    const float age = t - pt.born;
    const float u = age / pt.life;
    const float env = u < 0.1f ? u / 0.1f : std::pow(1.0f - (u - 0.1f) / 0.9f, 1.4f);
    if (env <= 0.01f) continue;
    splat(ctx, pt.pos.x, pt.pos.y, pt.pos.z, pt.color[0] * env, pt.color[1] * env,
          pt.color[2] * env);
  }
}

}  // namespace

extern const Pattern kFireworks = {"fireworks", init, render};

}  // namespace cube
