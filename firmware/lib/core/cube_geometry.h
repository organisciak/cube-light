#pragma once
#include <cstdint>

#include "cube_config.h"

// Port of src/shared/geometry.ts. The (x,y,z) -> LED index math runs once
// into a lookup table on layout change; per-voxel access is a table read.

namespace cube {

struct Layout {
  bool flipX = false;
  bool flipY = false;
  bool flipZ = false;
  /** LED n corresponds to wire-position (n + ledOffset) mod NUM_LEDS. */
  int ledOffset = 0;
};

class Geometry {
 public:
  Geometry() { rebuild(Layout{}); }

  void rebuild(const Layout& layout);

  uint16_t idx(int x, int y, int z) const {
    return table_[(z * CUBE_N + y) * CUBE_N + x];
  }

 private:
  uint16_t table_[NUM_LEDS];
};

}  // namespace cube
