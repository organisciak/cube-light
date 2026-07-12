#pragma once
#include <cstdint>

// WLED-style automatic brightness limiter (ABL). Estimates the current the
// frame would draw and returns a scale factor that keeps the total within
// the power supply's budget. Pure math — testable on the host.

namespace cube {

/**
 * Estimate the current draw of a frame in milliamps.
 *
 * perLedFullWhiteMA: what one LED draws at full white (all three channels
 * at 255). 12V WS2811 "seed" pixel strings are ~15mA; 5V WS2812 strips are
 * ~40-55mA. idlePerLedMA covers the IC's quiescent draw with the pixel off.
 */
float estimateCurrentMA(const uint8_t* rgb, int numLeds, float perLedFullWhiteMA,
                        float idlePerLedMA);

/**
 * Scale factor 0..1 to apply to the frame so the estimated draw fits within
 * budgetMA. Returns 1 when the frame already fits or budgetMA <= 0
 * (limiter disabled). The idle draw can't be scaled away, so the scale
 * applies to the modulated portion only — mirroring WLED's ABL.
 */
float currentLimitScale(const uint8_t* rgb, int numLeds, float perLedFullWhiteMA,
                        float idlePerLedMA, float budgetMA);

}  // namespace cube
