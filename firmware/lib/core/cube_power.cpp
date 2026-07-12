#include "cube_power.h"

namespace cube {

float estimateCurrentMA(const uint8_t* rgb, int numLeds, float perLedFullWhiteMA,
                        float idlePerLedMA) {
  // Sum all channel values; full white = numLeds * 3 * 255.
  uint32_t sum = 0;
  const int n = numLeds * 3;
  for (int i = 0; i < n; i++) sum += rgb[i];
  const float modulated =
      (float)sum / (float)(3 * 255) * perLedFullWhiteMA;  // "full-white LED equivalents"
  return numLeds * idlePerLedMA + modulated;
}

float currentLimitScale(const uint8_t* rgb, int numLeds, float perLedFullWhiteMA,
                        float idlePerLedMA, float budgetMA) {
  if (budgetMA <= 0) return 1.0f;
  const float idle = numLeds * idlePerLedMA;
  const float est = estimateCurrentMA(rgb, numLeds, perLedFullWhiteMA, idlePerLedMA);
  if (est <= budgetMA) return 1.0f;
  const float headroom = budgetMA - idle;
  if (headroom <= 0) return 0.0f;  // budget can't even cover quiescent draw
  return headroom / (est - idle);
}

}  // namespace cube
