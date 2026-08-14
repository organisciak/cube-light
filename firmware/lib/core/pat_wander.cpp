// Wander (firmware-original) — index-walk grown into a proper pattern: a
// single bright pixel strolling voxel-to-voxel through the cube's interior
// instead of along the wire. Speed can follow the music's level or tempo,
// beats make it turn a corner, and the ambient direction-change frequency is
// its own knob. A short fading tail shows where it's been, and beats can
// split the R/B channels off sideways — comic-misprint chromatic aberration.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

const int STEP[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};

constexpr int kMaxTail = 24;

int s_x, s_y, s_z;
int s_dir;
float s_progress;
float s_lastBeat;
bool s_turnPending;
int s_abAxis;  // axis the beat's R/B fringes split along
int8_t s_trail[kMaxTail][3];  // most recent first
int s_trailLen;

void init(PatternCtx& ctx) {
  s_x = s_y = s_z = CUBE_N / 2;
  s_dir = (int)(frand() * 6);
  s_progress = 0;
  s_lastBeat = 0;
  s_turnPending = false;
  s_abAxis = 0;
  s_trailLen = 0;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

inline bool inBounds(int x, int y, int z) {
  return x >= 0 && x < CUBE_N && y >= 0 && y < CUBE_N && z >= 0 && z < CUBE_N;
}

// Max-blend one channel at a voxel (aberration fringes can land on trail
// voxels; keep the brighter of the two instead of overwriting).
inline void maxCh(PatternCtx& ctx, uint8_t* buffer, int x, int y, int z, int ch,
                  float v) {
  if (!inBounds(x, y, z)) return;
  uint8_t* px = buffer + ctx.idx(x, y, z) * 3 + ch;
  const uint8_t b = (uint8_t)std::lround(v);
  if (b > *px) *px = b;
}

// Pick a new direction perpendicular to the current one that stays in
// bounds (a corner always leaves at least two choices).
int pickTurn() {
  int options[4];
  int count = 0;
  for (int d = 0; d < 6; d++) {
    if (d / 2 == s_dir / 2) continue;  // same axis: not a turn
    if (!inBounds(s_x + STEP[d][0], s_y + STEP[d][1], s_z + STEP[d][2])) continue;
    options[count++] = d;
  }
  return count > 0 ? options[(int)(frand() * count)] : s_dir;
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;

  const float speed = p.num("speed", 6.0f);
  const char* speedFrom = p.str("speedFrom", "none");
  const float speedGain = p.num("speedGain", 1.5f);
  const float turnChance = clamp01(p.num("turnChance", 0.25f));
  const bool beatTurn = p.boolean("beatTurn", true);
  const int tail = (int)std::fmax(0.0f, std::fmin((float)kMaxTail, std::floor(p.num("tail", 4.0f))));
  const float cr = p.num("r", 255.0f);
  const float cg = p.num("g", 255.0f);
  const float cb = p.num("b", 255.0f);
  const char* paletteName = p.str("palette", "none");
  const bool useP = paletteActive(paletteName);
  const PaletteRef pal = resolvePalette(paletteName, p, ctx.t);

  const float beatAb = p.num("beatAberration", 1.5f);

  // Beat rising edge queues a turn (consumed at the next voxel step) and
  // picks which sideways axis this beat's misprint fringes split along.
  const float beat = audio.beat;
  if (beat > 0.5f && s_lastBeat <= 0.5f) {
    if (beatTurn) s_turnPending = true;
    s_abAxis = (s_dir / 2 + 1 + (int)(frand() * 2)) % 3;  // not the travel axis
  }
  s_lastBeat = beat;
  // Comic-misprint chromatic aberration: each beat splits the R and B
  // channels apart from G along s_abAxis; the split rides the beat envelope
  // so it snaps on and settles back as the beat fades.
  const int split = (int)std::lround(beatAb * beat);
  const int adx = s_abAxis == 0 ? split : 0;
  const int ady = s_abAxis == 1 ? split : 0;
  const int adz = s_abAxis == 2 ? split : 0;

  s_progress += speed * audioSpeedMult(audio, speedFrom, speedGain) *
                std::fmax(0.0f, std::fmin(0.1f, ctx.dt));
  // Cap catch-up so a hitch can't teleport the pixel across the cube.
  if (s_progress > 4.0f) s_progress = 4.0f;

  while (s_progress >= 1.0f) {
    s_progress -= 1.0f;
    // Turn if the beat asked for one, by ambient chance, or when the wall
    // ahead forces it.
    const bool blocked = !inBounds(s_x + STEP[s_dir][0], s_y + STEP[s_dir][1],
                                   s_z + STEP[s_dir][2]);
    if (blocked || s_turnPending || frand() < turnChance) {
      s_dir = pickTurn();
      s_turnPending = false;
    }
    // Remember where we were, then step.
    for (int k = std::min(s_trailLen, kMaxTail - 1); k > 0; k--) {
      s_trail[k][0] = s_trail[k - 1][0];
      s_trail[k][1] = s_trail[k - 1][1];
      s_trail[k][2] = s_trail[k - 1][2];
    }
    s_trail[0][0] = (int8_t)s_x;
    s_trail[0][1] = (int8_t)s_y;
    s_trail[0][2] = (int8_t)s_z;
    if (s_trailLen < kMaxTail) s_trailLen++;
    s_x += STEP[s_dir][0];
    s_y += STEP[s_dir][1];
    s_z += STEP[s_dir][2];
  }

  std::memset(buffer, 0, NUM_LEDS * 3);

  // Tail first (oldest dimmest), head last at full brightness.
  const int shown = std::min(tail, s_trailLen);
  for (int k = shown - 1; k >= 0; k--) {
    const float fall = 1.0f - (float)(k + 1) / (tail + 1);
    float pr = cr, pg = cg, pb = cb;
    if (useP) {
      uint8_t rgb[3];
      samplePalette(pal, (float)k / std::fmax(1, tail), ctx.t, rgb);
      pr = rgb[0];
      pg = rgb[1];
      pb = rgb[2];
    }
    const int tx = s_trail[k][0], ty = s_trail[k][1], tz = s_trail[k][2];
    maxCh(ctx, buffer, tx + adx, ty + ady, tz + adz, 0, pr * fall);
    maxCh(ctx, buffer, tx, ty, tz, 1, pg * fall);
    maxCh(ctx, buffer, tx - adx, ty - ady, tz - adz, 2, pb * fall);
  }
  maxCh(ctx, buffer, s_x + adx, s_y + ady, s_z + adz, 0, cr);
  maxCh(ctx, buffer, s_x, s_y, s_z, 1, cg);
  maxCh(ctx, buffer, s_x - adx, s_y - ady, s_z - adz, 2, cb);
}

}  // namespace

extern const Pattern kWander = {"wander", init, render};

}  // namespace cube
