// Rail grind (firmware-original) — first-person ride down a neon rail, the
// cube as your viewport: depth runs along +y, the rail vanishes ahead in
// perspective and always passes underfoot at the near face's center. The
// rail is a procedural curve (drifting sines); the camera's heading chases
// the local slope with adjustable lag, so upcoming turns swing across the
// view and recenter as "you" carve into them. Loudness makes the oncoming
// line more jagged, and each beat drops a sudden swerve into the rail a few
// layers ahead — you watch it ride in, then lurch through it.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

constexpr int kMaxKinks = 8;
constexpr float kKinkWidth = 1.2f;   // world voxels a swerve takes to complete
constexpr float kPersp = 4.0f;       // perspective strength: proj = P/(depth+P)

struct Kink {
  float s;        // arc position of the swerve
  float ax, az;   // lateral / vertical jog it adds (world voxels)
  bool on;
};

float s_s0;             // camera arc position
float s_ph[4];          // random sine phases
float s_hx, s_hz;       // smoothed camera heading (slope per world voxel)
float s_baseX, s_baseZ; // folded-in offset of expired swerves
float s_lastBeat;
Kink s_kinks[kMaxKinks];

// Rail centerline at arc position s. ampX/ampZ already include the loudness
// jag; kinks add near-step jogs (logistic, so the curve stays smooth enough
// to ride).
float railAt(float s, float amp, float freq, const float* ph, float base,
             bool vert) {
  float v = base + amp * (std::sin(s * freq + ph[0]) +
                          0.6f * std::sin(s * freq * 1.73f + ph[1]));
  for (int i = 0; i < kMaxKinks; i++) {
    if (!s_kinks[i].on) continue;
    const float a = vert ? s_kinks[i].az : s_kinks[i].ax;
    if (a == 0) continue;
    v += a / (1.0f + std::exp(-(s - s_kinks[i].s) / kKinkWidth));
  }
  return v;
}

void init(PatternCtx& ctx) {
  s_s0 = 0;
  for (int i = 0; i < 4; i++) s_ph[i] = frand() * 6.2831853f;
  s_hx = s_hz = 0;
  s_baseX = s_baseZ = 0;
  s_lastBeat = 0;
  for (int i = 0; i < kMaxKinks; i++) s_kinks[i].on = false;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
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

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;

  const float speed = p.num("speed", 8.0f);
  const char* speedFrom = p.str("speedFrom", "none");
  const float speedGain = p.num("speedGain", 1.5f);
  const float curviness = p.num("curviness", 4.0f);
  const float turnLen = std::fmax(2.0f, p.num("turnLen", 14.0f));
  const float vertAmount = p.num("vertAmount", 2.0f);
  const float camLag = p.num("camLag", 0.4f);
  const float jagGain = p.num("jagGain", 1.0f);
  const float beatKink = p.num("beatKink", 3.0f);
  const float depthStep = std::fmax(0.25f, p.num("depthStep", 1.5f));
  const float fade = p.num("fade", 0.8f);
  const float glow = clamp01(p.num("glow", 0.25f));
  const float cr = p.num("r", 80.0f);
  const float cg = p.num("g", 220.0f);
  const float cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  const float freq = 6.2831853f / turnLen;
  const float ampX = curviness * (1.0f + jagGain * audio.level);
  const float ampZ = vertAmount * (1.0f + jagGain * audio.level);

  // Beat rising edge: drop a swerve into the rail near the far edge of view.
  const float beat = audio.beat;
  if (beat > 0.5f && s_lastBeat <= 0.5f && beatKink > 0) {
    int slot = -1;
    for (int i = 0; i < kMaxKinks; i++)
      if (!s_kinks[i].on) { slot = i; break; }
    if (slot >= 0) {
      const float amp = beatKink * (0.6f + 0.4f * frand()) * (frand() < 0.5f ? -1.0f : 1.0f);
      const bool vertical = frand() < 0.3f;
      s_kinks[slot] = {s_s0 + (CUBE_N - 1) * depthStep * 0.9f,
                       vertical ? 0 : amp, vertical ? amp : 0, true};
    }
  }
  s_lastBeat = beat;

  const float dt = std::fmax(0.0f, std::fmin(0.1f, ctx.dt));
  s_s0 += speed * audioSpeedMult(audio, speedFrom, speedGain) * dt;

  // Retire swerves well behind the camera; a passed kink's logistic has
  // saturated to its amplitude, so folding it into the base offset is
  // seamless.
  for (int i = 0; i < kMaxKinks; i++) {
    if (s_kinks[i].on && s_kinks[i].s < s_s0 - 24.0f) {
      s_baseX += s_kinks[i].ax;
      s_baseZ += s_kinks[i].az;
      s_kinks[i].on = false;
    }
  }

  // Camera rides the rail; heading chases the local slope with lag, so a
  // curve first slides sideways across the view, then straightens out.
  const float camX = railAt(s_s0, ampX, freq, s_ph, s_baseX, false);
  const float camZ = railAt(s_s0, ampZ, freq, s_ph + 2, s_baseZ, true);
  const float slopeX = railAt(s_s0 + 0.5f, ampX, freq, s_ph, s_baseX, false) -
                       railAt(s_s0 - 0.5f, ampX, freq, s_ph, s_baseX, false);
  const float slopeZ = railAt(s_s0 + 0.5f, ampZ, freq, s_ph + 2, s_baseZ, true) -
                       railAt(s_s0 - 0.5f, ampZ, freq, s_ph + 2, s_baseZ, true);
  const float a = camLag < 0.02f ? 1.0f : 1.0f - std::exp(-dt / camLag);
  s_hx += (slopeX - s_hx) * a;
  s_hz += (slopeZ - s_hz) * a;

  std::memset(buffer, 0, NUM_LEDS * 3);

  // March out along the rail, three samples per depth layer for continuity.
  constexpr int kSamp = CUBE_N * 3;
  for (int i = 0; i < kSamp; i++) {
    const float u = (float)i * (CUBE_N - 1) / (kSamp - 1);  // view depth 0..9
    const float s = s_s0 + u * depthStep;
    const float proj = kPersp / (u + kPersp);
    const float relX = railAt(s, ampX, freq, s_ph, s_baseX, false) - camX -
                       s_hx * u * depthStep;
    const float relZ = railAt(s, ampZ, freq, s_ph + 2, s_baseZ, true) - camZ -
                       s_hz * u * depthStep;
    const int x = (int)std::lround(4.5f + relX * proj);
    const int z = (int)std::lround(4.5f + relZ * proj);
    const int y = (int)std::lround(u);
    float pr = cr, pg = cg, pb = cb;
    if (useP) {
      uint8_t rgb[3];
      samplePalette(pal, u / (CUBE_N - 1), ctx.t, rgb);
      pr = rgb[0];
      pg = rgb[1];
      pb = rgb[2];
    }
    const float fall = std::pow(fade, u);
    splatMax(ctx, buffer, x, y, z, pr * fall, pg * fall, pb * fall);
    if (glow > 0) {
      const float gf = fall * glow;
      splatMax(ctx, buffer, x - 1, y, z, pr * gf, pg * gf, pb * gf);
      splatMax(ctx, buffer, x + 1, y, z, pr * gf, pg * gf, pb * gf);
      splatMax(ctx, buffer, x, y, z - 1, pr * gf, pg * gf, pb * gf);
      splatMax(ctx, buffer, x, y, z + 1, pr * gf, pg * gf, pb * gf);
    }
  }
}

}  // namespace

extern const Pattern kRailGrind = {"rail-grind", init, render};

}  // namespace cube
