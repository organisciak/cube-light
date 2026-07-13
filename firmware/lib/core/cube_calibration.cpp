#include "cube_calibration.h"

#include <cstring>

namespace cube {
namespace {

// Closed-form inverse of ledForLayout — the serpentine wire math runs both
// ways, so the disambiguation step needs no lookup tables (ESP32 DRAM is
// precious).
void coordsForLed(const Layout& l, uint16_t led, int& x, int& y, int& z) {
  const int N = CUBE_N;
  const int wirePos = (led + l.ledOffset) % NUM_LEDS;
  const int s = wirePos / N;                    // global string index
  const int p = wirePos % N;                    // position within string
  const int zi = s % 2 == 0 ? N - 1 - p : p;    // even string: down, odd: up
  const int r = s / N;                          // row (= y)
  const int sir = s % N;                        // string-in-row
  const int xi = r % 2 == 0 ? sir : N - 1 - sir;
  x = l.flipX ? N - 1 - xi : xi;
  y = l.flipY ? N - 1 - r : r;
  z = l.flipZ ? N - 1 - zi : zi;
}

int chooseDisambiguatingLed(const CalResult& r, const CalSample* samples, int count) {
  static bool taken[NUM_LEDS];
  std::memset(taken, 0, sizeof(taken));
  for (int i = 0; i < count; i++) taken[samples[i].led] = true;

  int bestLed = -1;
  int bestScore = -1;
  for (int led = 0; led < NUM_LEDS; led++) {
    if (taken[led]) continue;
    // Count distinct positions this LED maps to across candidates.
    int packed[8];
    int score = 0;
    for (int c = 0; c < r.candidateCount; c++) {
      int x, y, z;
      coordsForLed(r.candidates[c], (uint16_t)led, x, y, z);
      const int pk = x + y * CUBE_N + z * CUBE_N * CUBE_N;
      bool dup = false;
      for (int c2 = 0; c2 < score; c2++) {
        if (packed[c2] == pk) { dup = true; break; }
      }
      if (!dup) packed[score++] = pk;
    }
    if (score > bestScore) {
      bestScore = score;
      bestLed = led;
      if (score == r.candidateCount) break;  // perfectly disambiguating
    }
  }
  return bestLed;
}

}  // namespace

CalResult solveCalibration(const CalSample* samples, int count) {
  CalResult r;
  r.candidateCount = 0;
  r.suggestedNextLed = -1;
  r.bestEffortMisses = 0x7fff;

  if (count == 0) {
    r.suggestedNextLed = 0;
    return r;
  }

  for (int fx = 0; fx <= 1; fx++) {
    for (int fy = 0; fy <= 1; fy++) {
      for (int fz = 0; fz <= 1; fz++) {
        // Pin ledOffset from the first sample, then verify the rest.
        Layout trial{fx != 0, fy != 0, fz != 0, 0};
        const CalSample& s0 = samples[0];
        const int wireIdx = ledForLayout(trial, s0.x, s0.y, s0.z);
        trial.ledOffset = ((wireIdx - s0.led) % NUM_LEDS + NUM_LEDS) % NUM_LEDS;

        int miss = 0;
        for (int i = 0; i < count; i++) {
          if (ledForLayout(trial, samples[i].x, samples[i].y, samples[i].z) !=
              samples[i].led)
            miss++;
        }
        if (miss == 0) r.candidates[r.candidateCount++] = trial;
        if (miss < r.bestEffortMisses) {
          r.bestEffortMisses = miss;
          r.bestEffort = trial;
        }
      }
    }
  }

  if (r.candidateCount > 1) {
    r.suggestedNextLed = chooseDisambiguatingLed(r, samples, count);
  }
  return r;
}

}  // namespace cube
