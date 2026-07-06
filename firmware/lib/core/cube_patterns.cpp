// Registry, port of src/shared/patterns/index.ts (on-chip subset).
#include <cstring>

#include "cube_pattern.h"

namespace cube {

extern const Pattern kSolid;
extern const Pattern kPlasma;
extern const Pattern kWavySheet;
extern const Pattern kRotatingPlanes;

const Pattern* const kPatterns[] = {
    &kWavySheet,
    &kPlasma,
    &kRotatingPlanes,
    &kSolid,
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
