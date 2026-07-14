#include "cube_palettes.h"

#include <cmath>
#include <cstring>

#include "cube_color.h"

namespace cube {

namespace {

const PaletteStop kFire[] = {
    {0.0f, 0, 0, 0},     {0.2f, 80, 0, 0},     {0.4f, 255, 30, 0},
    {0.65f, 255, 150, 0}, {0.85f, 255, 240, 60}, {1.0f, 255, 255, 220},
};
const PaletteStop kArctic[] = {
    {0.0f, 4, 8, 22}, {0.4f, 30, 80, 180}, {0.7f, 80, 200, 240}, {1.0f, 240, 250, 255},
};
const PaletteStop kSunset[] = {
    {0.0f, 40, 0, 60},    {0.3f, 180, 30, 90},  {0.55f, 255, 100, 60},
    {0.8f, 255, 200, 80}, {1.0f, 255, 240, 200},
};
const PaletteStop kForest[] = {
    {0.0f, 4, 12, 6}, {0.4f, 10, 80, 20}, {0.75f, 80, 200, 40}, {1.0f, 220, 240, 120},
};
const PaletteStop kOcean[] = {
    {0.0f, 0, 8, 30}, {0.4f, 10, 50, 120}, {0.7f, 40, 180, 200}, {1.0f, 200, 255, 255},
};
const PaletteStop kMonoRed[] = {
    {0.0f, 0, 0, 0}, {0.5f, 200, 30, 30}, {1.0f, 255, 230, 200},
};
const PaletteStop kMonoBlue[] = {
    {0.0f, 0, 0, 0}, {0.5f, 30, 80, 220}, {1.0f, 220, 240, 255},
};
const PaletteStop kSpectrum[] = {
    {0.0f, 200, 0, 200}, {0.25f, 50, 0, 255}, {0.5f, 0, 200, 255},
    {0.75f, 50, 255, 50}, {1.0f, 255, 200, 0},
};
const PaletteStop kCyberpunk[] = {
    {0.0f, 10, 0, 30},    {0.2f, 120, 0, 140},  {0.4f, 255, 30, 160},
    {0.6f, 180, 60, 255}, {0.8f, 0, 220, 255},  {1.0f, 180, 255, 240},
};

struct NamedGradient {
  const char* name;
  const PaletteStop* stops;
  int count;
};

const NamedGradient kGradients[] = {
    {"fire", kFire, 6},         {"arctic", kArctic, 4},   {"sunset", kSunset, 5},
    {"forest", kForest, 4},     {"ocean", kOcean, 4},     {"mono_red", kMonoRed, 3},
    {"mono_blue", kMonoBlue, 3}, {"spectrum", kSpectrum, 5}, {"cyberpunk", kCyberpunk, 6},
};
constexpr int kGradientCount = sizeof(kGradients) / sizeof(kGradients[0]);

// Cycle order + timing mirror palettes.ts.
const char* const kCycleList[] = {"cyberpunk", "sunset", "arctic", "forest",
                                  "spectrum", "fire",   "ocean"};
constexpr int kCycleCount = 7;
constexpr float kCyclePeriodS = 8.0f;
constexpr float kCycleFadeS = 1.5f;

void sampleGradient(const PaletteStop* stops, int count, float t, uint8_t out[3]) {
  const float tt = clamp01(t);
  for (int i = 1; i < count; i++) {
    if (tt <= stops[i].pos) {
      const PaletteStop& a = stops[i - 1];
      const PaletteStop& b = stops[i];
      const float span = b.pos - a.pos;
      const float f = span > 0 ? (tt - a.pos) / span : (tt - a.pos);
      out[0] = (uint8_t)std::lround(a.r + (b.r - a.r) * f);
      out[1] = (uint8_t)std::lround(a.g + (b.g - a.g) * f);
      out[2] = (uint8_t)std::lround(a.b + (b.b - a.b) * f);
      return;
    }
  }
  out[0] = stops[count - 1].r;
  out[1] = stops[count - 1].g;
  out[2] = stops[count - 1].b;
}

const NamedGradient* findGradient(const char* name) {
  for (int i = 0; i < kGradientCount; i++) {
    if (std::strcmp(kGradients[i].name, name) == 0) return &kGradients[i];
  }
  return nullptr;
}

}  // namespace

const char* const kPaletteNames[] = {
    "none",     "fire",     "arctic",   "sunset",    "forest", "ocean",
    "mono_red", "mono_blue", "rainbow", "spectrum",  "cyberpunk", "cycle",
};
const int kPaletteNameCount = sizeof(kPaletteNames) / sizeof(kPaletteNames[0]);

bool paletteActive(const char* name) {
  if (!name || !name[0]) return false;
  if (std::strcmp(name, "none") == 0) return false;
  if (std::strcmp(name, "rainbow") == 0 || std::strcmp(name, "cycle") == 0) return true;
  return findGradient(name) != nullptr;
}

PaletteRef resolvePalette(const char* name) {
  if (name) {
    if (std::strcmp(name, "rainbow") == 0) return {PaletteKind::Rainbow, nullptr, 0};
    if (std::strcmp(name, "cycle") == 0) return {PaletteKind::Cycle, nullptr, 0};
    if (const NamedGradient* g = findGradient(name)) {
      return {PaletteKind::Gradient, g->stops, g->count};
    }
  }
  return {PaletteKind::Rainbow, nullptr, 0};  // TS getPalette() fallback
}

void samplePalette(const PaletteRef& ref, float t, float now, uint8_t out[3]) {
  switch (ref.kind) {
    case PaletteKind::Gradient:
      sampleGradient(ref.stops, ref.stopCount, t, out);
      return;
    case PaletteKind::Rainbow:
      hsvToRgb(t, 1.0f, 1.0f, out);
      return;
    case PaletteKind::Cycle: {
      const float phase = now / kCyclePeriodS;
      const int i = ((int)phase) % kCycleCount;
      const int j = (i + 1) % kCycleCount;
      const float localT = (phase - std::floor(phase)) * kCyclePeriodS;
      const NamedGradient* ga = findGradient(kCycleList[i]);
      sampleGradient(ga->stops, ga->count, t, out);
      if (localT <= kCyclePeriodS - kCycleFadeS) return;
      const float mix = (localT - (kCyclePeriodS - kCycleFadeS)) / kCycleFadeS;
      uint8_t b[3];
      const NamedGradient* gb = findGradient(kCycleList[j]);
      sampleGradient(gb->stops, gb->count, t, b);
      out[0] = (uint8_t)std::lround(out[0] * (1 - mix) + b[0] * mix);
      out[1] = (uint8_t)std::lround(out[1] * (1 - mix) + b[1] * mix);
      out[2] = (uint8_t)std::lround(out[2] * (1 - mix) + b[2] * mix);
      return;
    }
  }
}

}  // namespace cube
