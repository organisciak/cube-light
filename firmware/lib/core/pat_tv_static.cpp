// TV-static glitch (firmware-original). Born from a happy accident: comet on
// white occasionally dropped full-R/G/B pixels that read like old CRT static.
// This does it on purpose: an all-black cube where random pixels flash white
// to the music, each with a chromatic-aberration cross — pure red, green, and
// blue arms shooting out in different directions — and the beat stretches the
// arms longer. Optional faint gray per-pixel noise turns the background into
// true "dead channel" static.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

const int STEP[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

struct Glitch {
  int8_t x, y, z;
  uint8_t dirR, dirG, dirB;  // distinct axis directions for the three arms
  float born;
  float life;
};

constexpr int kMaxGlitches = 24;
Glitch s_glitches[kMaxGlitches];
int s_glitchCount = 0;
float s_lastT = 0;
float s_lastBeat = 0;
float s_spawnFrac = 0;

void spawnGlitch(float t, float life) {
  if (s_glitchCount >= kMaxGlitches) return;
  Glitch& g = s_glitches[s_glitchCount++];
  g.x = (int8_t)(frand() * CUBE_N);
  g.y = (int8_t)(frand() * CUBE_N);
  g.z = (int8_t)(frand() * CUBE_N);
  g.dirR = (uint8_t)(frand() * 6);
  do g.dirG = (uint8_t)(frand() * 6); while (g.dirG == g.dirR);
  do g.dirB = (uint8_t)(frand() * 6); while (g.dirB == g.dirR || g.dirB == g.dirG);
  g.born = t;
  g.life = life * (0.6f + frand() * 0.8f);
}

void init(PatternCtx& ctx) {
  s_glitchCount = 0;
  s_lastT = ctx.t;
  s_lastBeat = 0;
  s_spawnFrac = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

inline void maxAt(uint8_t* buffer, int i, float r, float g, float b) {
  const uint8_t rr = (uint8_t)std::lround(std::fmin(255.0f, r));
  const uint8_t gg = (uint8_t)std::lround(std::fmin(255.0f, g));
  const uint8_t bb = (uint8_t)std::lround(std::fmin(255.0f, b));
  if (rr > buffer[i]) buffer[i] = rr;
  if (gg > buffer[i + 1]) buffer[i + 1] = gg;
  if (bb > buffer[i + 2]) buffer[i + 2] = bb;
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;

  const int N = CUBE_N;
  const float spawnRate = p.num("spawnRate", 3.0f);
  const float beatSpawn = p.num("beatSpawn", 5.0f);
  const float life = std::fmax(0.05f, p.num("lifeSec", 0.3f));
  const float armLen = p.num("armLen", 1.0f);
  const float beatArm = p.num("beatArm", 2.0f);
  const float aberration = clamp01(p.num("aberration", 0.8f));
  const float grayLevel = p.num("gray", 0.0f);
  const float grayFlicker = clamp01(p.num("grayFlicker", 0.5f));

  const float beat = audio.beat;

  // Ambient trickle + a burst on every beat.
  s_spawnFrac += spawnRate * dt;
  int toSpawn = (int)s_spawnFrac;
  s_spawnFrac -= toSpawn;
  if (beat > 0.5f && s_lastBeat <= 0.5f) toSpawn += (int)std::lround(beatSpawn);
  s_lastBeat = beat;
  for (int s = 0; s < toSpawn; s++) spawnGlitch(t, life);

  // Background: black, or per-pixel gray noise like a dead analog channel.
  if (grayLevel > 0.5f) {
    for (int i = 0; i < NUM_LEDS; i++) {
      const uint8_t v =
          (uint8_t)std::lround(grayLevel * (1.0f - grayFlicker * frand()));
      buffer[i * 3] = v;
      buffer[i * 3 + 1] = v;
      buffer[i * 3 + 2] = v;
    }
  } else {
    std::memset(buffer, 0, NUM_LEDS * 3);
  }

  // Glitches: white flash at the center, R/G/B arms in distinct directions.
  // The beat envelope stretches the arms outward live.
  const float arm = armLen + beatArm * beat;
  int alive = 0;
  for (int gi = 0; gi < s_glitchCount; gi++) {
    Glitch& g = s_glitches[gi];
    const float u = (t - g.born) / g.life;
    if (u >= 1.0f) continue;
    // Snap on fast, fade out — a flashbulb, not a swell.
    const float env = u < 0.15f ? u / 0.15f : 1.0f - (u - 0.15f) / 0.85f;

    maxAt(buffer, ctx.idx(g.x, g.y, g.z) * 3, 255 * env, 255 * env, 255 * env);

    const int dirs[3] = {g.dirR, g.dirG, g.dirB};
    for (int ch = 0; ch < 3; ch++) {
      const int* st = STEP[dirs[ch]];
      const int cells = (int)std::ceil(arm);
      for (int k = 1; k <= cells; k++) {
        const int cx = g.x + st[0] * k, cy = g.y + st[1] * k, cz = g.z + st[2] * k;
        if (cx < 0 || cx >= N || cy < 0 || cy >= N || cz < 0 || cz >= N) break;
        // Fractional coverage at the arm tip so beat growth looks smooth.
        const float cover = std::fmin(1.0f, arm - (k - 1));
        const float v = 255.0f * env * aberration * cover * (1.0f - 0.15f * k);
        if (v <= 1.0f) break;
        const int i = ctx.idx(cx, cy, cz) * 3;
        maxAt(buffer, i, ch == 0 ? v : 0, ch == 1 ? v : 0, ch == 2 ? v : 0);
      }
    }
    s_glitches[alive++] = g;
  }
  s_glitchCount = alive;
}

}  // namespace

extern const Pattern kTvStatic = {"tv-static", init, render};

}  // namespace cube
