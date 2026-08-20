// Registry, port of src/shared/patterns/index.ts (on-chip subset).
#include <cstring>

#include "cube_pattern.h"

namespace cube {

extern const Pattern kSolid;
extern const Pattern kWavySheet;
extern const Pattern kRotatingPlanes;
extern const Pattern kRain;
extern const Pattern kComet;
extern const Pattern kCloud;
extern const Pattern kAudioRipple;
extern const Pattern kSpectrumDiscs;
extern const Pattern kText3d;
extern const Pattern kImage3d;
extern const Pattern kTvStatic;
extern const Pattern kWander;
extern const Pattern kIndexWalk;
extern const Pattern kLitPixel;
extern const Pattern kBuildMap;
extern const Pattern kFireworks;
extern const Pattern kSnake3d;
extern const Pattern kSnakeCal;
extern const Pattern kPacman3d;
extern const Pattern kBarEq;
extern const Pattern kSpiral;
extern const Pattern kSpinCube;
extern const Pattern kBounce;
extern const Pattern kOrbit;
extern const Pattern kScan;
extern const Pattern kRailGrind;
extern const Pattern kRezTunnel;
extern const Pattern kDoubleHelix;

const Pattern* const kPatterns[] = {
    &kWavySheet,
    &kRotatingPlanes,
    &kRain,
    &kComet,
    &kCloud,
    &kAudioRipple,
    &kSpectrumDiscs,
    &kBarEq,
    &kSpiral,
    &kSpinCube,
    &kBounce,
    &kOrbit,
    &kScan,
    &kRailGrind,
    &kRezTunnel,
    &kDoubleHelix,
    &kTvStatic,
    &kWander,
    &kText3d,
    &kImage3d,
    &kFireworks,
    &kSnake3d,
    &kSnakeCal,
    &kPacman3d,
    &kSolid,
    &kIndexWalk,
    &kLitPixel,
    &kBuildMap,
};
const int kPatternCount = sizeof(kPatterns) / sizeof(kPatterns[0]);
const char* const kDefaultPatternId = "wavy-sheet";

const Pattern* findPattern(const char* id) {
  for (int i = 0; i < kPatternCount; i++) {
    if (std::strcmp(kPatterns[i]->id, id) == 0) return kPatterns[i];
  }
  return nullptr;
}

}  // namespace cube
