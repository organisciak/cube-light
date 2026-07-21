#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Preset persistence (issue cube-eq5.1). Thin storage layer over LittleFS,
// mounted on the "spiffs" partition from partitions_cube.csv (128KB). One
// JSON file per preset under /presets/<slug>.json — chosen over a single NVS
// blob so the later import/export issue can copy files directly and to dodge
// NVS's ~4000-byte putString cap. Lives under src/ (not lib/core) because it
// pulls in Arduino-only LittleFS/ArduinoJson, which the pure-C++ native
// harness can't compile.
//
// A preset document (param snapshot + playlist metadata):
//   {"name":str,"pattern":patternId,"priority":0-5,"dwellSec":num,
//    "params":{key:value,...}}
// Param values follow the /api/params conventions: bare numbers, quoted
// strings/enums/palettes, true/false bools. priority + dwellSec are stored
// now for the playlist issues (cube-eq5.2/.3) but not acted on here.

namespace cube {

struct PresetMeta {
  String name;
  String pattern;
  int priority;
  float dwellSec;
};

constexpr int kMaxPresets = 40;

void presetsBegin();                         // mount LittleFS (idempotent)
String presetSlug(const String& name);       // filesystem slug for a display name
int presetList(PresetMeta* out, int max);    // metadata only; returns count
bool presetRead(const String& name, JsonDocument& doc);
bool presetWrite(const String& name, const JsonDocument& doc);
bool presetDelete(const String& name);

}  // namespace cube
