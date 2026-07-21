// Snake direction-calibration visual (issue cube-wuw). Not a game pattern —
// driven by the /api/snakecal wizard. For the horizontal cube direction named
// by the "dir" param (SnakeDir 0..3 = XP,XN,YP,YN), it outlines the target
// face in a distinct color and drives a bright white arrow through the middle
// horizontal layer pointing that way, so the player can answer "which button
// points toward the lit edge?".
//
// Every LED is written each frame (memset up front); no persistence-by-skip.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_pattern.h"

namespace cube {
namespace {

void render(PatternCtx& ctx) {
  uint8_t* buf = ctx.buffer;
  std::memset(buf, 0, NUM_LEDS * 3);
  const int N = CUBE_N;
  const int dir = std::max(0, std::min(3, (int)std::lround(ctx.params->num("dir", 0.0f))));
  const int axis = dir / 2;            // 0 = x, 1 = y
  const int sign = (dir % 2 == 0) ? 1 : -1;  // XP/YP -> +, XN/YN -> -
  const int face = sign > 0 ? N - 1 : 0;
  const int mid = N / 2;
  const float t = ctx.t;
  const float pulse = 0.55f + 0.45f * std::sin(t * 4.0f);

  auto add = [&](int x, int y, int z, int r, int g, int b) {
    if (x < 0 || x >= N || y < 0 || y >= N || z < 0 || z >= N) return;
    const int i = ctx.idx(x, y, z) * 3;
    buf[i] = (uint8_t)std::min(255, (int)buf[i] + r);
    buf[i + 1] = (uint8_t)std::min(255, (int)buf[i + 1] + g);
    buf[i + 2] = (uint8_t)std::min(255, (int)buf[i + 2] + b);
  };
  // (along, perp, z) in cube coords, where `along` is the target axis and
  // `perp` the other horizontal axis. Lets the arrow math ignore x-vs-y.
  auto addAP = [&](int along, int perp, int z, int r, int g, int b) {
    if (axis == 0)
      add(along, perp, z, r, g, b);
    else
      add(perp, along, z, r, g, b);
  };

  // Distinct color per prompted direction, for the target face outline.
  const int col[4][3] = {{255, 40, 40}, {255, 150, 0}, {40, 255, 90}, {0, 170, 255}};
  const int cr = (int)std::lround(col[dir][0] * pulse);
  const int cg = (int)std::lround(col[dir][1] * pulse);
  const int cb = (int)std::lround(col[dir][2] * pulse);

  // Outline the target face (its 4 edges) in the distinct color. The face is
  // the plane at coord `face` on the target axis; its border is where either
  // perpendicular coord is at an extreme.
  for (int u = 0; u < N; u++) {
    for (int v = 0; v < N; v++) {
      const bool edge = (u == 0 || u == N - 1 || v == 0 || v == N - 1);
      if (!edge) continue;
      if (axis == 0)
        add(face, u, v, cr, cg, cb);  // x fixed; u=y, v=z
      else
        add(u, face, v, cr, cg, cb);  // y fixed; u=x, v=z
    }
  }

  // Bright white arrow through the middle horizontal layer, center -> face.
  const int white = (int)std::lround(230 * (0.7f + 0.3f * pulse));
  const int tip = face;  // arrow tip lands on the lit face
  // Shaft: from the cube center out to one voxel shy of the tip.
  for (int a = mid; a != tip; a += sign) {
    addAP(a, mid, mid, white, white, white);
  }
  // Arrowhead: a 3D "V" that fans out (in the perpendicular horizontal AND
  // vertical directions) as it recedes from the tip, so it reads from any side.
  for (int k = 0; k <= 3; k++) {
    const int a = tip - sign * k;
    addAP(a, mid - k, mid, white, white, white);
    addAP(a, mid + k, mid, white, white, white);
    addAP(a, mid, mid - k, white, white, white);
    addAP(a, mid, mid + k, white, white, white);
  }
}

}  // namespace

extern const Pattern kSnakeCal = {"snake-cal", nullptr, render};

}  // namespace cube
