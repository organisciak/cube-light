// Port of src/shared/patterns/dayCycle.ts.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_pattern.h"

namespace cube {
namespace {

struct RGBf {
  float r, g, b;
};

inline RGBf lerpC(const RGBf& a, const RGBf& b, float t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t};
}

// Ambient sky color keyed by time-of-day (0=midnight, 0.25=sunrise,
// 0.5=noon, 0.75=sunset, 1=midnight).
struct SkyStop {
  float pos;
  RGBf c;
};
const SkyStop kSkyStops[] = {
    {0.00f, {4, 6, 24}},     {0.20f, {50, 30, 80}},    {0.27f, {180, 90, 60}},
    {0.35f, {110, 165, 225}}, {0.50f, {60, 140, 240}},  {0.65f, {110, 160, 220}},
    {0.73f, {210, 100, 50}},  {0.80f, {60, 30, 80}},    {1.00f, {4, 6, 24}},
};
constexpr int kSkyStopCount = sizeof(kSkyStops) / sizeof(kSkyStops[0]);

RGBf sampleSky(float t) {
  const float tt = t - std::floor(t);
  for (int i = 1; i < kSkyStopCount; i++) {
    if (tt <= kSkyStops[i].pos) {
      const float span = kSkyStops[i].pos - kSkyStops[i - 1].pos;
      const float f = span > 0 ? (tt - kSkyStops[i - 1].pos) / span : 0;
      return lerpC(kSkyStops[i - 1].c, kSkyStops[i].c, f);
    }
  }
  return kSkyStops[kSkyStopCount - 1].c;
}

constexpr float kPi = 3.14159265358979f;

float s_lastT = 0;
float s_cycleT = 0;

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_cycleT = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;

  const int N = CUBE_N;
  const float c = (N - 1) / 2.0f;
  const char upAxis = p.str("upAxis", "z")[0];
  const float peak = p.num("peak", 4.0f);
  const float noonR = p.num("hillR", 50.0f);
  const float noonG = p.num("hillG", 200.0f);
  const float noonB = p.num("hillB", 70.0f);
  const float sunSize = std::fmax(0.5f, p.num("sunSize", 2.5f));
  const float moonBright = clamp01(p.num("moonBright", 0.5f));
  const bool manual = p.str("mode", "auto")[0] == 'm';

  float tod;
  if (manual) {
    const float tm = p.num("time", 0.5f);
    tod = tm - std::floor(tm);
  } else {
    const float cycleSec = std::fmax(1.0f, p.num("cycleSec", 60.0f));
    s_cycleT = std::fmod(s_cycleT + dt / cycleSec, 1.0f);
    tod = s_cycleT;
  }

  // Celestial geometry in abstract (up, east, side) coords: the sun rides a
  // half-circle in the (up, east) plane; the moon trails half a cycle behind.
  const float angle = (tod - 0.25f) * kPi * 2.0f;
  const float sunAlt = std::sin(angle);
  const float arcR = c + 0.8f;
  const float sunU = c + arcR * std::sin(angle);
  const float sunE = c + arcR * std::cos(angle);
  const float sunS = c;
  const float mAngle = angle + kPi;
  const float moonAlt = std::sin(mAngle);
  const float moonU = c + arcR * std::sin(mAngle);
  const float moonE = c + arcR * std::cos(mAngle);
  const float moonS = c;

  const float dayness = clamp01((sunAlt + 0.15f) / 0.4f);
  const float twilight = clamp01(1.0f - std::fabs(sunAlt) / 0.25f);
  const float moonLight = std::fmax(0.0f, moonAlt) * moonBright;

  const RGBf skyBase = sampleSky(tod);
  const RGBf sunWarm = {255, 170, 80};
  const RGBf sunBright = {255, 240, 200};
  const RGBf sunColor = lerpC(sunWarm, sunBright, clamp01(sunAlt));
  const RGBf moonColor = {220, 230, 255};

  // Lit hill color: noon green attenuated by overall lighting + twilight
  // warm tint + cool moonlight tint.
  const float lighting = 0.05f + 0.95f * dayness + 0.35f * moonLight;
  RGBf hillLit = {noonR * lighting + 50.0f * twilight + 8.0f * moonLight,
                  noonG * lighting - 30.0f * twilight,
                  noonB * lighting + 30.0f * moonLight};

  // Paraboloid hill mask, tapering to zero beyond the corners.
  const float cornerR = std::sqrt(c * c * 2.0f);
  const float hillBaseRSq = (cornerR * 1.2f) * (cornerR * 1.2f);

  std::memset(buffer, 0, NUM_LEDS * 3);

  for (int u = 0; u < N; u++) {
    for (int e = 0; e < N; e++) {
      for (int s = 0; s < N; s++) {
        const float de = e - c;
        const float ds = s - c;
        const float dHillSq = de * de + ds * ds;
        const float heightHere = peak * std::fmax(0.0f, 1.0f - dHillSq / hillBaseRSq);
        const float hillT = clamp01(heightHere - u + 0.5f);

        float r = skyBase.r;
        float g = skyBase.g;
        float b = skyBase.b;

        // Sun contribution (visible while not deep below horizon).
        if (sunAlt > -0.3f) {
          const float su = u - sunU, se = e - sunE, ss = s - sunS;
          const float sd = std::sqrt(su * su + se * se + ss * ss);
          const float horizonFade =
              sunAlt > 0 ? 1.0f : std::fmax(0.0f, (sunAlt + 0.3f) / 0.3f);
          const float glow = std::exp(-sd / sunSize) * horizonFade;
          const float core = std::fmax(0.0f, 1.8f - sd) * (sunAlt > -0.1f ? 1.0f : 0.0f);
          const float sunMix = clamp01(glow * 0.9f + core);
          r = r * (1 - sunMix) + sunColor.r * sunMix;
          g = g * (1 - sunMix) + sunColor.g * sunMix;
          b = b * (1 - sunMix) + sunColor.b * sunMix;
        }

        // Moon contribution.
        if (moonAlt > -0.1f && moonBright > 0) {
          const float mu = u - moonU, me = e - moonE, ms = s - moonS;
          const float md = std::sqrt(mu * mu + me * me + ms * ms);
          const float horizonFade = clamp01((moonAlt + 0.1f) / 0.2f);
          const float mGlow = std::exp(-md / (sunSize * 0.7f)) * horizonFade;
          const float mCore = std::fmax(0.0f, 1.5f - md) * horizonFade;
          const float mMix = clamp01((mGlow * 0.5f + mCore) * moonBright);
          r = r * (1 - mMix) + moonColor.r * mMix;
          g = g * (1 - mMix) + moonColor.g * mMix;
          b = b * (1 - mMix) + moonColor.b * mMix;
        }

        // Blend toward hill where the mask is set.
        if (hillT > 0) {
          r = r * (1 - hillT) + hillLit.r * hillT;
          g = g * (1 - hillT) + hillLit.g * hillT;
          b = b * (1 - hillT) + hillLit.b * hillT;
        }

        // Map abstract (up, east, side) -> physical (x, y, z).
        const int i =
            (upAxis == 'z' ? ctx.idx(e, s, u)
                           : upAxis == 'x' ? ctx.idx(u, e, s) : ctx.idx(e, u, s)) *
            3;
        buffer[i] = (uint8_t)std::fmax(0.0f, std::fmin(255.0f, std::lround(r)));
        buffer[i + 1] = (uint8_t)std::fmax(0.0f, std::fmin(255.0f, std::lround(g)));
        buffer[i + 2] = (uint8_t)std::fmax(0.0f, std::fmin(255.0f, std::lround(b)));
      }
    }
  }
}

}  // namespace

extern const Pattern kDayCycle = {"day-cycle", init, render};

}  // namespace cube
