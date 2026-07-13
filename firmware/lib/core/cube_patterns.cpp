// Registry, port of src/shared/patterns/index.ts (on-chip subset).
#include <cstring>

#include "cube_pattern.h"

namespace cube {

extern const Pattern kSolid;
extern const Pattern kPlasma;
extern const Pattern kWavySheet;
extern const Pattern kRotatingPlanes;
extern const Pattern kRain;
extern const Pattern kFire;
extern const Pattern kComet;
extern const Pattern kCloud;
extern const Pattern kAudioRipple;
extern const Pattern kSpectrumDiscs;
extern const Pattern kDayCycle;
extern const Pattern kLife3d;
extern const Pattern kText3d;
extern const Pattern kIndexWalk;
extern const Pattern kLitPixel;
extern const Pattern kFireworks;
extern const Pattern kSnake3d;
extern const Pattern kPacman3d;
extern const Pattern kBarEq;

const Pattern* const kPatterns[] = {
    &kWavySheet,
    &kPlasma,
    &kRotatingPlanes,
    &kRain,
    &kFire,
    &kComet,
    &kCloud,
    &kAudioRipple,
    &kSpectrumDiscs,
    &kBarEq,
    &kDayCycle,
    &kLife3d,
    &kText3d,
    &kFireworks,
    &kSnake3d,
    &kPacman3d,
    &kSolid,
    &kIndexWalk,
    &kLitPixel,
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
