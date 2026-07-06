#include "cube_color.h"

#include <cmath>

namespace cube {

void hsvToRgb(float h, float s, float v, uint8_t out[3]) {
  const float hh = h - std::floor(h);
  const int i = (int)(hh * 6.0f);
  const float f = hh * 6.0f - (float)i;
  const float p = v * (1.0f - s);
  const float q = v * (1.0f - f * s);
  const float t = v * (1.0f - (1.0f - f) * s);
  float r = 0, g = 0, b = 0;
  switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
  out[0] = (uint8_t)std::lround(r * 255.0f);
  out[1] = (uint8_t)std::lround(g * 255.0f);
  out[2] = (uint8_t)std::lround(b * 255.0f);
}

}  // namespace cube
