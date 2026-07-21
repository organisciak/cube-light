#include "cube_presets.h"

#include <LittleFS.h>

namespace cube {

static bool mounted = false;

// Map a display name to a filesystem-safe path. Distinct names that reduce to
// the same slug collide (last save wins) — acceptable for this foundation;
// the display name is preserved verbatim inside the file's "name" field.
static String pathFor(const String& name) {
  String slug;
  for (size_t i = 0; i < name.length() && slug.length() < 40; i++) {
    const char c = name[i];
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_') {
      slug += c;
    } else if (c == ' ') {
      slug += '_';
    }
  }
  if (slug.length() == 0) slug = "preset";
  return "/presets/" + slug + ".json";
}

void presetsBegin() {
  if (mounted) return;
  // format-on-fail so a fresh/blank spiffs partition becomes usable.
  mounted = LittleFS.begin(true);
  if (mounted && !LittleFS.exists("/presets")) LittleFS.mkdir("/presets");
}

int presetList(PresetMeta* out, int max) {
  if (!mounted) return 0;
  File dir = LittleFS.open("/presets");
  if (!dir || !dir.isDirectory()) return 0;
  int n = 0;
  for (File f = dir.openNextFile(); f && n < max; f = dir.openNextFile()) {
    JsonDocument doc;
    if (!deserializeJson(doc, f)) {
      out[n].name = doc["name"] | "";
      out[n].pattern = doc["pattern"] | "";
      out[n].priority = doc["priority"] | 3;
      out[n].dwellSec = doc["dwellSec"] | 20.0f;
      if (out[n].name.length()) n++;
    }
    f.close();
  }
  dir.close();
  return n;
}

bool presetRead(const String& name, JsonDocument& doc) {
  if (!mounted) return false;
  File f = LittleFS.open(pathFor(name), "r");
  if (!f) return false;
  const bool ok = !deserializeJson(doc, f);
  f.close();
  return ok;
}

bool presetWrite(const String& name, const JsonDocument& doc) {
  if (!mounted) return false;
  File f = LittleFS.open(pathFor(name), "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

bool presetDelete(const String& name) {
  if (!mounted) return false;
  return LittleFS.remove(pathFor(name));
}

}  // namespace cube
