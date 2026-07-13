#pragma once
#include <cstdint>

#include "cube_geometry.h"

// Port of src/shared/calibration.ts: search the 8 flip combinations, pin
// ledOffset from the first sample, verify the rest, and (when several
// layouts survive) suggest the LED whose position best disambiguates them.

namespace cube {

struct CalSample {
  uint16_t led;
  uint8_t x, y, z;
};

struct CalResult {
  int candidateCount;      // layouts matching every sample (0..8)
  Layout candidates[8];
  int suggestedNextLed;    // -1 = none needed / none available
  Layout bestEffort;       // when candidateCount == 0: closest layout
  int bestEffortMisses;
};

CalResult solveCalibration(const CalSample* samples, int count);

}  // namespace cube
