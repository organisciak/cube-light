#include "cube_geometry.h"

namespace cube {

void Geometry::rebuild(const Layout& layout) {
  const int N = CUBE_N;
  for (int z = 0; z < N; z++) {
    for (int y = 0; y < N; y++) {
      for (int x = 0; x < N; x++) {
        const int xi = layout.flipX ? N - 1 - x : x;
        const int yi = layout.flipY ? N - 1 - y : y;
        const int zi = layout.flipZ ? N - 1 - z : z;
        const int r = yi;                                   // row index (= y)
        const int sir = r % 2 == 0 ? xi : N - 1 - xi;       // string-in-row, row x-reversal
        const int s = r * N + sir;                          // global string index
        const int p = s % 2 == 0 ? N - 1 - zi : zi;         // even string: down, odd: up
        const int wirePos = s * N + p;
        const int led = ((wirePos - layout.ledOffset) % NUM_LEDS + NUM_LEDS) % NUM_LEDS;
        table_[(z * N + y) * N + x] = (uint16_t)led;
      }
    }
  }
}

}  // namespace cube
