// Port of src/shared/patterns/audioRipple.ts. Ripple list becomes a fixed
// pool (64 far exceeds any realistic simultaneous ripple count).
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

struct Ripple {
  float age;
  float pos;  // palette position / hue
  float intensity;
};

constexpr int kMaxRipples = 64;
Ripple s_ripples[kMaxRipples];
int s_rippleCount = 0;
float s_lastT = 0;
float s_lastBeat = 0;
float s_lastLevel = 0;

void spawn(float pos, float intensity) {
  if (s_rippleCount >= kMaxRipples) return;
  s_ripples[s_rippleCount++] = {0.0f, pos, intensity};
}

void init(PatternCtx& ctx) {
  s_rippleCount = 0;
  s_lastT = ctx.t;
  s_lastBeat = 0;
  s_lastLevel = 0;
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
  const float speed = p.num("speed", 5.0f);
  const float thickness = std::fmax(0.2f, p.num("thickness", 1.0f));
  const float fade = std::fmax(0.5f, p.num("fade", 2.0f));
  const float autoSpawnRate = p.num("autoSpawn", 0.0f);
  const float beatThreshold = p.num("beatThreshold", 0.3f);
  const float levelTrigger = p.num("levelTrigger", 0.2f);
  const float sat = p.num("sat", 0.85f);
  const char* paletteName = p.str("palette", "spectrum");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  // Spawn on the beat envelope's rising edge, or on a sharp level rise
  // (catches fast transients the beat detector misses).
  const float beat = audio.beat;
  const float level = audio.level;
  const bool beatRising = beat > beatThreshold && s_lastBeat <= beatThreshold;
  const bool levelRising = levelTrigger > 0 && level - s_lastLevel > levelTrigger;
  if (beatRising || levelRising) {
    const float bass = audio.bands[0];
    const float mid = audio.bands[AUDIO_BANDS / 2];
    const float treble = audio.bands[AUDIO_BANDS - 1];
    const float total = bass + mid + treble + 1e-6f;
    // Palette position weighted by band balance: bass->0, mid->0.5, treble->1.
    const float pos = (mid * 0.5f + treble * 1.0f) / total;
    spawn(pos, 0.7f + 0.3f * level);
  }
  s_lastBeat = beat;
  // Decay the cached level slowly so the rising-edge detector keeps firing on
  // each new transient rather than once per loud section.
  s_lastLevel = std::fmax(level, s_lastLevel - dt * 1.5f);

  if (autoSpawnRate > 0 && frand() < autoSpawnRate * dt) {
    spawn(frand(), 0.7f);
  }

  std::memset(buffer, 0, NUM_LEDS * 3);
  const float c = (N - 1) / 2.0f;

  int alive = 0;
  for (int ri = 0; ri < s_rippleCount; ri++) {
    Ripple& rp = s_ripples[ri];
    rp.age += dt;
    if (rp.age > fade) continue;
    const float rad = rp.age * speed;
    if (rad > N * 1.8f) continue;
    const float lifeFrac = 1.0f - rp.age / fade;
    const float intensity = rp.intensity * lifeFrac;
    const float pp = rp.pos - std::floor(rp.pos);
    uint8_t base[3];
    float rr, gg, bb;
    if (useP) {
      samplePalette(pal, pp, t, base);
      rr = std::lround(base[0] * intensity);
      gg = std::lround(base[1] * intensity);
      bb = std::lround(base[2] * intensity);
    } else {
      hsvToRgb(pp, sat, intensity, base);
      rr = base[0];
      gg = base[1];
      bb = base[2];
    }

    for (int z = 0; z < N; z++) {
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          const float dx = x - c;
          const float dy = y - c;
          const float dz = z - c;
          const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
          const float d = std::fabs(dist - rad);
          if (d > thickness) continue;
          const float fall = 1.0f - d / thickness;
          const int i = ctx.idx(x, y, z) * 3;
          const uint8_t vr = (uint8_t)std::lround(rr * fall);
          const uint8_t vg = (uint8_t)std::lround(gg * fall);
          const uint8_t vb = (uint8_t)std::lround(bb * fall);
          if (vr > buffer[i]) buffer[i] = vr;
          if (vg > buffer[i + 1]) buffer[i + 1] = vg;
          if (vb > buffer[i + 2]) buffer[i + 2] = vb;
        }
      }
    }
    s_ripples[alive++] = rp;
  }
  s_rippleCount = alive;
}

}  // namespace

extern const Pattern kAudioRipple = {"audio-ripple", init, render};

}  // namespace cube
