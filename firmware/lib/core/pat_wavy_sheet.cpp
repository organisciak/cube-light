// Port of src/shared/patterns/wavySheet.ts. Comments about the audio mapping
// live in the TS original; this file keeps the math identical.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"

namespace cube {
namespace {

constexpr float kPi = 3.14159265358979f;

// Phase accumulator for the in-plane rotation, kept across frames so the
// rotation rate can vary (audio-reactive) without phase jumps.
float s_lastT = 0;
float s_rotPhase = 0;
// Smoothed audio inputs for the audioDecay param: attack is instant (hits
// still land), release decays over ~audioDecay seconds. Zero = raw/twitchy.
float s_level = 0, s_bass = 0, s_mid = 0, s_treble = 0, s_beat = 0;

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_rotPhase = 0;
  s_level = s_bass = s_mid = s_treble = s_beat = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

inline float attackRelease(float smoothed, float raw, float tau, float dt) {
  if (raw >= smoothed || tau <= 0.0f) return raw;
  return smoothed + (raw - smoothed) * (1.0f - std::exp(-dt / tau));
}

inline void blendMax(uint8_t* buffer, int i, float r, float g, float b) {
  const uint8_t rr = (uint8_t)std::lround(r);
  const uint8_t gg = (uint8_t)std::lround(g);
  const uint8_t bb = (uint8_t)std::lround(b);
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
  std::memset(buffer, 0, NUM_LEDS * 3);

  const int N = CUBE_N;
  const char axis = p.str("axis", "z")[0];
  const float baseZ = p.num("baseZ", 4.0f);
  const float amp = p.num("amp", 1.2f);
  const float speed = p.num("speed", 0.6f);
  const float wl = std::fmax(0.5f, p.num("wavelength", 8.0f));
  const float thickness = std::fmax(0.1f, p.num("thickness", 1.0f));
  const float edgeSoft = std::fmax(0.0f, p.num("edgeSoft", 1.0f));
  const float audioDecay = p.num("audioDecay", 0.0f);
  const float levelGain = p.num("levelGain", 2.0f);
  const float bassGain = p.num("bassGain", 2.5f);
  const float midGain = p.num("midGain", 0.5f);
  const float trebleGain = p.num("trebleGain", 0.6f);
  const float beatGain = p.num("beatGain", 1.5f);
  const float rotateSpeed = p.num("rotateSpeed", 0.02f);
  const bool rotateAudio = p.boolean("rotateAudio", false);
  const float rotateLevelGain = p.num("rotateLevelGain", 1.5f);
  const float rotateBeatGain = p.num("rotateBeatGain", 2.0f);
  const float cr = p.num("r", 220.0f);
  const float cg = p.num("g", 220.0f);
  const float cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName);

  const float k = (kPi * 2.0f) / wl;
  const float wt = t * speed * kPi * 2.0f;
  s_level = attackRelease(s_level, audio.level, audioDecay, dt);
  s_bass = attackRelease(s_bass, audio.bands[0], audioDecay, dt);
  s_mid = attackRelease(s_mid, audio.bands[AUDIO_BANDS / 2], audioDecay, dt);
  s_treble = attackRelease(s_treble, audio.bands[AUDIO_BANDS - 1], audioDecay, dt);
  s_beat = attackRelease(s_beat, audio.beat, audioDecay, dt);
  const float bass = s_bass;
  const float mid = s_mid;
  const float treble = s_treble;
  const float beat = s_beat;
  const float audioAmp = amp * (1.0f + s_level * levelGain + beat * beatGain);

  const float rotRate =
      rotateAudio
          ? rotateSpeed * (1.0f + clamp01(s_level) * rotateLevelGain) +
                rotateSpeed * beat * rotateBeatGain
          : rotateSpeed;
  s_rotPhase += rotRate * kPi * 2.0f * dt;
  const float cosR = std::cos(s_rotPhase);
  const float sinR = std::sin(s_rotPhase);

  for (int a = 0; a < N; a++) {
    for (int b = 0; b < N; b++) {
      const float da0 = a - (N - 1) / 2.0f;
      const float db0 = b - (N - 1) / 2.0f;
      // Rotate the in-plane sample point so the wavefronts spin around the
      // sheet's center while the LED grid itself stays put.
      const float da = da0 * cosR - db0 * sinR;
      const float db = da0 * sinR + db0 * cosR;
      const float r = std::sqrt(da * da + db * db);
      const float surface =
          baseZ +
          audioAmp *
              (std::sin(k * da + wt) * 0.55f +
               std::sin(k * db - wt * 0.8f) * (0.55f + midGain * mid) +
               bass * bassGain * std::cos(k * r * 0.7f - wt * 1.5f) +
               // Center beat bump. Scaled by beatGain so zeroing the slider
               // truly silences it (4/3 keeps the default-1.5 look at 2.0).
               beat * beatGain * (4.0f / 3.0f) * std::exp(-r * 0.3f) +
               trebleGain * treble * std::sin(k * 4.0f * (da + db) + wt * 5.0f));

      const int hMin = (int)std::fmax(0.0f, std::floor(surface - thickness));
      const int hMax = (int)std::fmin((float)(N - 1), std::ceil(surface + thickness));
      for (int h = hMin; h <= hMax; h++) {
        const float d = std::fabs(h - surface);
        if (d > thickness) continue;
        // edgeSoft shapes the falloff: 1 = linear (original), 0 = hard slab,
        // >1 = progressively softer fringes.
        const float fall = std::pow(1.0f - d / thickness, edgeSoft);
        float pr = cr, pg = cg, pb = cb;
        if (useP) {
          uint8_t rgb[3];
          samplePalette(pal, (float)h / (N - 1), t, rgb);
          pr = rgb[0];
          pg = rgb[1];
          pb = rgb[2];
        }
        const int i =
            (axis == 'z' ? ctx.idx(a, b, h)
                         : axis == 'y' ? ctx.idx(a, h, b) : ctx.idx(h, a, b)) *
            3;
        blendMax(buffer, i, pr * fall, pg * fall, pb * fall);
      }
    }
  }
}

}  // namespace

extern const Pattern kWavySheet = {"wavy-sheet", init, render};

}  // namespace cube
