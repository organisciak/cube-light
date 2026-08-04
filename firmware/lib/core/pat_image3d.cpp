// image-3d — shows the shared 10x10 image (album art via /api/image) on all
// four vertical faces, text-ring style, over a fading background glow in the
// image's average color. The glow (and, partially, the art) throbs to the
// beat via the shared cube_throb.h module.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_image.h"
#include "cube_pattern.h"
#include "cube_throb.h"

namespace cube {
namespace {

AudioThrob s_throb;

void init(PatternCtx& ctx) {
  s_throb.reset();
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

inline void writeMax(uint8_t* buffer, int i, float r, float g, float b) {
  const int off = i * 3;
  const uint8_t rr = (uint8_t)(r > 255 ? 255 : r);
  const uint8_t gg = (uint8_t)(g > 255 ? 255 : g);
  const uint8_t bb = (uint8_t)(b > 255 ? 255 : b);
  if (rr > buffer[off]) buffer[off] = rr;
  if (gg > buffer[off + 1]) buffer[off + 1] = gg;
  if (bb > buffer[off + 2]) buffer[off + 2] = bb;
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const int N = CUBE_N;
  std::memset(buffer, 0, NUM_LEDS * 3);

  const char axis = p.str("axis", "z")[0];
  const float level = clamp01(p.num("level", 1.0f));
  const float bgLevel = clamp01(p.num("bgLevel", 0.12f));
  const float bgFade = clamp01(p.num("bgFade", 0.7f));
  // Fraction of the throb that reaches the art itself; the background glow
  // always takes the full throb.
  const float imgThrob = clamp01(p.num("imgThrob", 0.25f));

  const float throb = s_throb.update(ctx);
  const float artMult = level * (1.0f - imgThrob * (1.0f - throb));

  uint8_t avg[3];
  cubeImageAverage(avg);
  const uint8_t* img = cubeImagePixels();

  // Background glow: the whole volume in the image's average color, fading
  // toward the top of the vertical axis, throbbing with the beat.
  if (bgLevel > 0) {
    for (int z = 0; z < N; z++) {
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          const int v = axis == 'z' ? z : axis == 'y' ? y : x;
          const float fade = 1.0f - bgFade * ((float)v / (N - 1));
          const float k = bgLevel * fade * throb;
          if (k <= 0) continue;
          writeMax(buffer, ctx.idx(x, y, z), avg[0] * k, avg[1] * k, avg[2] * k);
        }
      }
    }
  }

  // The art on all four vertical faces (same face walk as text-3d ring mode).
  for (int gy = 0; gy < IMG_N; gy++) {
    const int v = N - 1 - gy;  // image top row -> high end of the vertical axis
    for (int gx = 0; gx < IMG_N; gx++) {
      const uint8_t* px = &img[(gy * IMG_N + gx) * 3];
      const float r = px[0] * artMult;
      const float g = px[1] * artMult;
      const float b = px[2] * artMult;
      const int h = gx;
      for (int face = 0; face < 4; face++) {
        int cx = 0, cy = 0, cz = 0;
        if (axis == 'z') {
          cz = v;
          if (face == 0) { cx = h; cy = 0; }
          else if (face == 1) { cx = N - 1; cy = h; }
          else if (face == 2) { cx = N - 1 - h; cy = N - 1; }
          else { cx = 0; cy = N - 1 - h; }
        } else if (axis == 'y') {
          cy = v;
          if (face == 0) { cx = h; cz = 0; }
          else if (face == 1) { cx = N - 1; cz = h; }
          else if (face == 2) { cx = N - 1 - h; cz = N - 1; }
          else { cx = 0; cz = N - 1 - h; }
        } else {
          cx = v;
          if (face == 0) { cy = h; cz = 0; }
          else if (face == 1) { cy = N - 1; cz = h; }
          else if (face == 2) { cy = N - 1 - h; cz = N - 1; }
          else { cy = 0; cz = N - 1 - h; }
        }
        writeMax(buffer, ctx.idx(cx, cy, cz), r, g, b);
      }
    }
  }
}

}  // namespace

extern const Pattern kImage3d = {"image-3d", init, render};

}  // namespace cube
