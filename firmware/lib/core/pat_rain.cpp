// Port of src/shared/patterns/rain.ts. Drop list becomes a fixed pool
// (kMaxDrops exceeds the worst-case live count at max spawn rate).
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

struct Drop {
  float x, y, z;
  float vz;
  float pos;  // palette position / hue
  float brightness;
};

constexpr int kMaxDrops = 512;
Drop s_drops[kMaxDrops];
int s_dropCount = 0;
float s_lastT = 0;
float s_pendingSpawnFrac = 0;

void init(PatternCtx& ctx) {
  s_dropCount = 0;
  s_lastT = ctx.t;
  s_pendingSpawnFrac = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;

  const int N = CUBE_N;
  const float spawnRate = p.num("spawnRate", 14.0f);
  const float fallSpeed = p.num("fallSpeed", 9.0f);
  const float speedJitter = clamp01(p.num("speedJitter", 0.4f));
  const float trail = std::fmax(0.5f, std::fmin(0.99f, p.num("trail", 0.85f)));
  const float basePos = p.num("pos", 0.6f);
  const float posJitter = clamp01(p.num("jitter", 0.06f));
  const float audioBoost = p.num("audioBoost", 2.0f);
  const char* paletteName = p.str("palette", "arctic");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  // Decay every voxel — gives the falling trail.
  for (int i = 0; i < NUM_LEDS * 3; i++) buffer[i] = (uint8_t)(buffer[i] * trail);

  // Accumulate fractional spawn count to hold the target rate at any fps.
  const float target = spawnRate * (1.0f + ctx.audio->level * audioBoost);
  s_pendingSpawnFrac += target * dt;
  int spawnCount = (int)s_pendingSpawnFrac;
  s_pendingSpawnFrac -= spawnCount;
  for (int s = 0; s < spawnCount && s_dropCount < kMaxDrops; s++) {
    Drop& d = s_drops[s_dropCount++];
    d.x = (float)(int)(frand() * N);
    d.y = (float)(int)(frand() * N);
    d.z = N - 0.5f + frand() * 0.5f;
    d.vz = -fallSpeed * (1.0f + (frand() - 0.5f) * 2.0f * speedJitter);
    d.pos = basePos + (frand() - 0.5f) * 2.0f * posJitter;
    d.brightness = 0.85f + frand() * 0.15f;
  }

  // Step + render, compacting dead drops in place.
  int alive = 0;
  for (int di = 0; di < s_dropCount; di++) {
    Drop& d = s_drops[di];
    d.z += d.vz * dt;
    if (d.z < -1.0f) continue;
    const int zi = (int)std::lround(d.z);
    if (zi >= 0 && zi < N) {
      const float pp = d.pos - std::floor(d.pos);
      uint8_t rgb[3];
      if (useP) {
        samplePalette(pal, pp, t, rgb);
        rgb[0] = (uint8_t)std::lround(rgb[0] * d.brightness);
        rgb[1] = (uint8_t)std::lround(rgb[1] * d.brightness);
        rgb[2] = (uint8_t)std::lround(rgb[2] * d.brightness);
      } else {
        hsvToRgb(pp, 0.85f, d.brightness, rgb);
      }
      const int i = ctx.idx((int)d.x, (int)d.y, zi) * 3;
      if (rgb[0] > buffer[i]) buffer[i] = rgb[0];
      if (rgb[1] > buffer[i + 1]) buffer[i + 1] = rgb[1];
      if (rgb[2] > buffer[i + 2]) buffer[i + 2] = rgb[2];
    }
    s_drops[alive++] = d;
  }
  s_dropCount = alive;
}

}  // namespace

extern const Pattern kRain = {"rain", init, render};

}  // namespace cube
