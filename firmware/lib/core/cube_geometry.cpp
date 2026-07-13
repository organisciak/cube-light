#include "cube_geometry.h"

#include <cstring>

namespace cube {

UpAxis upAxisFromString(const char* s) {
  if (!s || !s[0]) return UpAxis::ZPos;
  const char axis = s[0];
  const bool neg = s[1] == '-';
  switch (axis) {
    case 'x': return neg ? UpAxis::XNeg : UpAxis::XPos;
    case 'y': return neg ? UpAxis::YNeg : UpAxis::YPos;
    default: return neg ? UpAxis::ZNeg : UpAxis::ZPos;
  }
}

const char* upAxisToString(UpAxis up) {
  switch (up) {
    case UpAxis::XPos: return "x+";
    case UpAxis::XNeg: return "x-";
    case UpAxis::YPos: return "y+";
    case UpAxis::YNeg: return "y-";
    case UpAxis::ZNeg: return "z-";
    default: return "z+";
  }
}

uint16_t ledForLayout(const Layout& layout, int x, int y, int z) {
  const int N = CUBE_N;
  const int xi = layout.flipX ? N - 1 - x : x;
  const int yi = layout.flipY ? N - 1 - y : y;
  const int zi = layout.flipZ ? N - 1 - z : z;
  const int r = yi;                                   // row index (= y)
  const int sir = r % 2 == 0 ? xi : N - 1 - xi;       // string-in-row, row x-reversal
  const int s = r * N + sir;                          // global string index
  const int p = s % 2 == 0 ? N - 1 - zi : zi;         // even string: down, odd: up
  const int wirePos = s * N + p;
  return (uint16_t)(((wirePos - layout.ledOffset) % NUM_LEDS + NUM_LEDS) % NUM_LEDS);
}

namespace {

// Proper rotations taking the design's +z to the chosen physical direction.
void orient(UpAxis up, int x, int y, int z, int& px, int& py, int& pz) {
  const int N1 = CUBE_N - 1;
  switch (up) {
    case UpAxis::ZNeg: px = x; py = N1 - y; pz = N1 - z; break;  // 180 deg about x
    case UpAxis::XPos: px = z; py = y; pz = N1 - x; break;       // -90 deg about y
    case UpAxis::XNeg: px = N1 - z; py = y; pz = x; break;       // +90 deg about y
    case UpAxis::YPos: px = x; py = z; pz = N1 - y; break;       // -90 deg about x
    case UpAxis::YNeg: px = x; py = N1 - z; pz = y; break;       // +90 deg about x
    default: px = x; py = y; pz = z; break;                      // ZPos: identity
  }
}

}  // namespace

void Geometry::rebuild(const Layout& layout, UpAxis up) {
  const int N = CUBE_N;
  for (int z = 0; z < N; z++) {
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        int px, py, pz;
        orient(up, x, y, z, px, py, pz);
        table_[(z * N + y) * N + x] = ledForLayout(layout, px, py, pz);
      }
    }
  }
}

}  // namespace cube
