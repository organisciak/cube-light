#include "cube_presets.h"

#include <LittleFS.h>

namespace cube {

static bool mounted = false;

// In-RAM metadata cache: presetList() would otherwise open + JSON-parse every
// file, which the playlist does on EVERY advance and the UI polls — a visible
// frame hitch at short dwells. Any write/delete invalidates it. Heap-allocated
// lazily: as a static array it would cost ~1.6KB of the (overflowing) DRAM
// static segment.
static PresetMeta* s_cache = nullptr;
static int s_cacheN = 0;
static bool s_cacheDirty = true;

// Map a display name to a filesystem-safe path. Distinct names that reduce to
// the same slug collide (last save wins) — acceptable for this foundation;
// the display name is preserved verbatim inside the file's "name" field.
String presetSlug(const String& name) {
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
  return slug;
}

static String pathFor(const String& name) {
  return "/presets/" + presetSlug(name) + ".json";
}

void presetsBegin() {
  if (mounted) return;
  // format-on-fail so a fresh/blank spiffs partition becomes usable.
  mounted = LittleFS.begin(true);
  if (mounted && !LittleFS.exists("/presets")) LittleFS.mkdir("/presets");
}

int presetList(PresetMeta* out, int max) {
  if (!mounted) return 0;
  if (!s_cache) s_cache = new PresetMeta[kMaxPresets];
  if (s_cacheDirty) {
    File dir = LittleFS.open("/presets");
    if (!dir || !dir.isDirectory()) return 0;
    s_cacheN = 0;
    for (File f = dir.openNextFile(); f && s_cacheN < kMaxPresets;
         f = dir.openNextFile()) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        s_cache[s_cacheN].name = doc["name"] | "";
        s_cache[s_cacheN].pattern = doc["pattern"] | "";
        s_cache[s_cacheN].priority = doc["priority"] | 3;
        s_cache[s_cacheN].dwellSec = doc["dwellSec"] | 20.0f;
        // Presets saved before the flag existed classify themselves.
        s_cache[s_cacheN].reactive =
            doc["reactive"].isNull()
                ? presetLooksReactive(s_cache[s_cacheN].pattern,
                                      doc["params"].as<JsonObjectConst>())
                : doc["reactive"].as<bool>();
        if (s_cache[s_cacheN].name.length()) s_cacheN++;
      }
      f.close();
    }
    dir.close();
    s_cacheDirty = false;
  }
  const int n = s_cacheN < max ? s_cacheN : max;
  for (int i = 0; i < n; i++) out[i] = s_cache[i];
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
  s_cacheDirty = true;
  return ok;
}

bool presetDelete(const String& name) {
  if (!mounted) return false;
  s_cacheDirty = true;
  return LittleFS.remove(pathFor(name));
}

bool presetLooksReactive(const String& patternId, JsonObjectConst params) {
  // These patterns are audio visualizers regardless of param values.
  if (patternId == "audio-ripple" || patternId == "spectrum-discs" ||
      patternId == "bar-eq")
    return true;
  if (params.isNull()) return false;
  // Any audio-drive param above zero counts. Key vocabulary across the spec
  // table: *Gain/gain, audio*, beat*, *Kick, *Pulse, *Sparkle, levelBoost.
  // beatThreshold is a trigger level, not a gain, so it's excluded.
  for (JsonPairConst kv : params) {
    String lk = kv.key().c_str();
    lk.toLowerCase();
    if (lk == "beatthreshold") continue;
    const bool audioKey = lk.indexOf("gain") >= 0 || lk.indexOf("audio") >= 0 ||
                          lk.indexOf("beat") >= 0 || lk.indexOf("kick") >= 0 ||
                          lk.indexOf("pulse") >= 0 || lk.indexOf("sparkle") >= 0 ||
                          lk == "levelboost";
    if (!audioKey) continue;
    if (kv.value().is<bool>()) {
      if (kv.value().as<bool>()) return true;
    } else if (kv.value().as<float>() > 0.0f) {
      return true;
    }
  }
  return false;
}

}  // namespace cube
