#pragma once
#include <cstdint>

#include "cube_config.h"

// Port of src/shared/geometry.ts. The (x,y,z) -> LED index math runs once
// into a lookup table on layout change; per-voxel access is a table read.
//
// Two independent knobs compose into the table:
//  - Layout: how the strings are physically wired (calibration).
//  - UpAxis: which physical direction the *design* treats as up ("basic
//    orientation" — patterns are authored z-up; this rotates the whole
//    design so +z lands on the chosen physical axis).

namespace cube {

struct Layout {
  bool flipX = false;
  bool flipY = false;
  bool flipZ = false;
  /** LED n corresponds to wire-position (n + ledOffset) mod NUM_LEDS. */
  int ledOffset = 0;
};

enum class UpAxis : uint8_t { ZPos, ZNeg, XPos, XNeg, YPos, YNeg };

/** Parse "z+","z-","x+","x-","y+","y-"; unknown -> ZPos. */
UpAxis upAxisFromString(const char* s);
const char* upAxisToString(UpAxis up);

/** Raw wiring math (no up-axis): design (x,y,z) -> LED index for a layout. */
uint16_t ledForLayout(const Layout& layout, int x, int y, int z);

class Geometry {
 public:
  Geometry() { rebuild(Layout{}, UpAxis::ZPos); }

  void rebuild(const Layout& layout, UpAxis up = UpAxis::ZPos);

  uint16_t idx(int x, int y, int z) const {
    return table_[(z * CUBE_N + y) * CUBE_N + x];
  }

 private:
  uint16_t table_[NUM_LEDS];
};

}  // namespace cube
