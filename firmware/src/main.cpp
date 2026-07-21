// ESP32 firmware entry point (Gledopto GL-C-618WL target).
//
// Status: skeleton, written ahead of hardware arrival — compiles under
// PlatformIO but is UNTESTED on a real board.
//
// BEFORE FIRST FLASH: open the stock WLED UI and record every pin mapping
// its settings pages show (LED GPIO, color order, button pin, mic pins,
// relay pin). Then set the CUBE_* build flags in platformio.ini to match.
//
// What it does:
//  - Settings persist in NVS (Preferences): WiFi credentials, AP/OTA
//    password, pattern, brightness, color order, power budget. Build flags
//    are only the first-boot defaults.
//  - Joins the configured WiFi; falls back to a WPA2 AP ("cube-light") if
//    no credentials or the join times out.
//  - Serves a phone-friendly control page at http://cube.local/ (pattern,
//    brightness, power budget, color order) and /wifi for network setup.
//  - Runs the shared pattern core at CUBE_FPS, applies brightness + a
//    WLED-style current limiter, pushes frames via NeoPixelBus (RMT).
//  - Listens for WLED DNRGB packets on UDP 21324; live packets override the
//    local pattern until their timeout lapses (same semantics as WLED), so
//    the Node dev server keeps working unchanged.
//  - Function button short-press cycles patterns (works with no network).
//  - ArduinoOTA (password-protected) for wireless reflashing.
//
// Not yet here: mic capture + FFT -> BeatDetector, per-pattern params over
// the API, /snake controller page, full React-app WS contract.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <NeoPixelBus.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>

#include "audio_capture.h"
#include "cube_calibration.h"
#include "cube_mod.h"
#include "cube_pacman.h"
#include "cube_palettes.h"
#include "cube_param_specs.h"
#include "cube_pattern.h"
#include "cube_presets.h"
#include "cube_snake.h"
#include "cube_power.h"
#include "web_ui.h"

#define CUBE_VERSION "0.2.0"

#ifndef CUBE_LED_PIN
#define CUBE_LED_PIN 16
#endif
// Second output: the cube's 1000-LED chain is cut at the wire midpoint and
// fed as two 500-LED runs (halves transmit time -> 60fps ceiling, halves
// data-error propagation, and pairs with power injection at both feed
// points). The second half must be fed at its ORIGINAL START (where LED
// CUBE_LED_SPLIT used to take data), keeping wire direction unchanged, so
// the geometry mapping stays valid. Set CUBE_LED_PIN2=-1 for one unbroken
// 1000-LED chain on CUBE_LED_PIN.
#ifndef CUBE_LED_PIN2
#define CUBE_LED_PIN2 12
#endif
#ifndef CUBE_LED_SPLIT
#define CUBE_LED_SPLIT 500  // LEDs on output 1; the rest go to output 2
#endif
#ifndef CUBE_FPS
#define CUBE_FPS 30
#endif
#ifndef CUBE_COLOR_ORDER
#define CUBE_COLOR_ORDER "RGB"
#endif
#ifndef CUBE_SUPPLY_MA
#define CUBE_SUPPLY_MA 10000
#endif
#ifndef CUBE_PER_LED_MA
#define CUBE_PER_LED_MA 15
#endif
#ifndef CUBE_IDLE_MA_PER_LED
#define CUBE_IDLE_MA_PER_LED 0.5f
#endif
#ifndef CUBE_BUTTON_PIN
#define CUBE_BUTTON_PIN 17  // 618WL function button (from stock WLED cfg); -1 disables
#endif
#ifndef CUBE_RELAY_PIN
// The 618WL has an energy-saving relay that cuts LED V+ when "off". Stock
// WLED drives it on GPIO18 (active high). If we don't raise it, the string
// stays dark no matter what we clock out. -1 disables.
#define CUBE_RELAY_PIN 18
#endif
#ifndef CUBE_AP_PASS
#define CUBE_AP_PASS "cubelight"  // default AP+OTA password; change via /wifi
#endif

using namespace cube;

// ---- persisted settings -----------------------------------------------------

struct Settings {
  String wifiSsid;
  String wifiPass;
  String apPass;      // WPA2 AP password, doubles as the OTA password
  String uiPass;      // optional HTTP auth for the settings console ("" = off)
  String patternId;
  String colorOrder;  // "RGB", "GRB", ...
  float brightness;   // 0..1
  uint32_t supplyMA;  // 0 = limiter off
  Layout layout;      // wiring calibration
  String upAxis;      // "z+","z-","x+","x-","y+","y-"
  int ledPin;         // output 1 GPIO
  int ledPin2;        // output 2 GPIO; -1 = single unbroken chain
  int ledSplit;       // LEDs on output 1 when split
  bool micLeft;       // PDM channel format
  float micSquelch;   // raw RMS below this = silence
  bool micEnabled;    // master toggle: false = patterns see silence
};

Preferences prefs;
Settings settings;

void loadSettings() {
  prefs.begin("cube", true);
  settings.wifiSsid = prefs.getString("ssid", "");
  settings.wifiPass = prefs.getString("pass", "");
  settings.apPass = prefs.getString("appass", CUBE_AP_PASS);
  settings.uiPass = prefs.getString("uipass", "");
  settings.patternId = prefs.getString("pattern", kDefaultPatternId);
  settings.colorOrder = prefs.getString("order", CUBE_COLOR_ORDER);
  settings.brightness = prefs.getFloat("bright", 1.0f);
  settings.supplyMA = prefs.getUInt("supply", CUBE_SUPPLY_MA);
  settings.layout.flipX = prefs.getBool("flipx", false);
  settings.layout.flipY = prefs.getBool("flipy", false);
  settings.layout.flipZ = prefs.getBool("flipz", false);
  settings.layout.ledOffset = prefs.getInt("ledoff", 0);
  settings.upAxis = prefs.getString("up", "z+");
  // Default is a single unbroken 1000-LED chain on output 1; set pin2/split
  // from the console once the chain is physically cut in half.
  settings.ledPin = prefs.getInt("ledpin", CUBE_LED_PIN);
  settings.ledPin2 = prefs.getInt("ledpin2", -1);
  settings.ledSplit = prefs.getInt("ledsplit", CUBE_LED_SPLIT);
  settings.micLeft = prefs.getBool("micleft", false);
  settings.micSquelch = prefs.getFloat("micsq", 60.0f);
  settings.micEnabled = prefs.getBool("micen", true);
  prefs.end();
}

void saveHardware() {
  prefs.begin("cube", false);
  prefs.putInt("ledpin", settings.ledPin);
  prefs.putInt("ledpin2", settings.ledPin2);
  prefs.putInt("ledsplit", settings.ledSplit);
  prefs.putBool("micleft", settings.micLeft);
  prefs.putFloat("micsq", settings.micSquelch);
  prefs.end();
}

void saveLayoutAndUp() {
  prefs.begin("cube", false);
  prefs.putBool("flipx", settings.layout.flipX);
  prefs.putBool("flipy", settings.layout.flipY);
  prefs.putBool("flipz", settings.layout.flipZ);
  prefs.putInt("ledoff", settings.layout.ledOffset);
  prefs.putString("up", settings.upAxis);
  prefs.end();
}

void saveSetting(const char* key, const String& v) {
  prefs.begin("cube", false);
  prefs.putString(key, v);
  prefs.end();
}
void saveSetting(const char* key, float v) {
  prefs.begin("cube", false);
  prefs.putFloat(key, v);
  prefs.end();
}
void saveSetting(const char* key, uint32_t v) {
  prefs.begin("cube", false);
  prefs.putUInt(key, v);
  prefs.end();
}

// ---- LED output ---------------------------------------------------------------

// Feature fixed at RGB; configured color order is a runtime permutation
// applied while copying into the strip. Pins and the single/dual split are
// runtime settings: strips live on the heap and are torn down + rebuilt by
// initStrips() when hardware config changes (NeoPixelBus releases its RMT
// channel on destruction). Two buses transmit concurrently when split.
using Strip1T = NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2811Method>;
using Strip2T = NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt1Ws2811Method>;
Strip1T* strip1 = nullptr;
Strip2T* strip2 = nullptr;
int stripSplit = NUM_LEDS;  // LEDs on output 1

void initStrips() {
  delete strip1;
  strip1 = nullptr;
  delete strip2;
  strip2 = nullptr;
  const bool dual = settings.ledPin2 >= 0 && settings.ledSplit > 0 &&
                    settings.ledSplit < NUM_LEDS;
  stripSplit = dual ? settings.ledSplit : NUM_LEDS;
  strip1 = new Strip1T(stripSplit, settings.ledPin);
  strip1->Begin();
  strip1->Show();
  if (dual) {
    strip2 = new Strip2T(NUM_LEDS - stripSplit, settings.ledPin2);
    strip2->Begin();
    strip2->Show();
  }
  Serial.printf("[led] pin1=%d n=%d%s\n", settings.ledPin, stripSplit,
                dual ? (" pin2=" + String(settings.ledPin2) + " n=" +
                        String(NUM_LEDS - stripSplit)).c_str()
                     : " (single chain)");
}

uint8_t colorPerm[3] = {0, 1, 2};  // perm[wireSlot] = source channel (0=R 1=G 2=B)

void applyColorOrder(const String& order) {
  for (int i = 0; i < 3 && i < (int)order.length(); i++) {
    colorPerm[i] = order[i] == 'G' ? 1 : order[i] == 'B' ? 2 : 0;
  }
}

void show(const uint8_t* rgb) {
  if (!strip1) return;
  const float limit = currentLimitScale(rgb, NUM_LEDS, CUBE_PER_LED_MA,
                                        CUBE_IDLE_MA_PER_LED, settings.supplyMA);
  const float k = limit * settings.brightness;
  for (int i = 0; i < NUM_LEDS; i++) {
    const uint8_t* px = rgb + i * 3;
    const RgbColor c((uint8_t)(px[colorPerm[0]] * k), (uint8_t)(px[colorPerm[1]] * k),
                     (uint8_t)(px[colorPerm[2]] * k));
    if (i < stripSplit) strip1->SetPixelColor(i, c);
    else if (strip2) strip2->SetPixelColor(i - stripSplit, c);
  }
  strip1->Show();
  if (strip2) strip2->Show();
}

// ---- pattern engine -----------------------------------------------------------

Geometry geo;

void applyGeometry() {
  geo.rebuild(settings.layout, upAxisFromString(settings.upAxis.c_str()));
}

Params params;
ModStore mods;  // per-param automatic modulation (cube-la3), current pattern only
AudioFrame audio;  // refreshed each frame from the mic capture task
uint8_t frame[NUM_LEDS * 3];
const Pattern* activePattern = nullptr;
int activePatternIdx = 0;
PatternCtx ctx{frame, &geo, 0, 0, &audio, &params};
uint32_t patternStartMs = 0;
float lastT = 0;

// Effective (override-or-default) value of one spec, as a String.
String effectiveParam(const ParamSpec& sp) {
  switch (sp.type) {
    case 1: return params.boolean(sp.key, sp.defNum != 0) ? "1" : "0";
    case 2:
    case 3:
    case 4: return String(params.str(sp.key, sp.defStr));
    default: return String(params.num(sp.key, sp.defNum), 3);
  }
}

void applyParamFromString(const ParamSpec& sp, const String& v) {
  switch (sp.type) {
    case 1: params.setBool(sp.key, v == "1" || v == "true"); break;
    case 2:
    case 3:
    case 4: params.setStr(sp.key, v.c_str()); break;
    default: params.setNum(sp.key, v.toFloat()); break;
  }
}

// Saved param overrides live in NVS as "key=value\n" blobs per pattern.
String paramsKeyFor(int idx) {
  // NVS keys cap at 15 chars; ids are unique within their first 13.
  return "pp" + String(kPatterns[idx]->id).substring(0, 13);
}

void loadPatternParams(int idx) {
  prefs.begin("cube", true);
  const String blob = prefs.getString(paramsKeyFor(idx).c_str(), "");
  prefs.end();
  if (blob.length() == 0) return;
  const PatternSpecs* ps = specsFor(kPatterns[idx]->id);
  if (!ps) return;
  int pos = 0;
  while (pos < (int)blob.length()) {
    int nl = blob.indexOf('\n', pos);
    if (nl < 0) nl = blob.length();
    const String line = blob.substring(pos, nl);
    const int eq = line.indexOf('=');
    if (eq > 0) {
      const String key = line.substring(0, eq);
      for (int i = 0; i < ps->count; i++) {
        if (key == ps->specs[i].key) {
          applyParamFromString(ps->specs[i], line.substring(eq + 1));
          break;
        }
      }
    }
    pos = nl + 1;
  }
}

void savePatternParams(int idx) {
  const PatternSpecs* ps = specsFor(kPatterns[idx]->id);
  if (!ps) return;
  String blob;
  for (int i = 0; i < ps->count; i++) {
    blob += ps->specs[i].key;
    blob += '=';
    blob += effectiveParam(ps->specs[i]);
    blob += '\n';
  }
  prefs.begin("cube", false);
  prefs.putString(paramsKeyFor(idx).c_str(), blob);
  prefs.end();
}

// Modulation configs live in NVS beside the param overrides, one blob per
// pattern, keyed "pm<id13>". Lines are "key=mode,min,max,rate,step".
String modKeyFor(int idx) {
  return "pm" + String(kPatterns[idx]->id).substring(0, 13);
}

void loadPatternMods(int idx) {
  mods.clear();
  prefs.begin("cube", true);
  const String blob = prefs.getString(modKeyFor(idx).c_str(), "");
  prefs.end();
  if (blob.length() == 0) return;
  int pos = 0;
  while (pos < (int)blob.length()) {
    int nl = blob.indexOf('\n', pos);
    if (nl < 0) nl = blob.length();
    const String line = blob.substring(pos, nl);
    pos = nl + 1;
    const int eq = line.indexOf('=');
    if (eq <= 0) continue;
    const String key = line.substring(0, eq);
    const String v = line.substring(eq + 1);
    const int c1 = v.indexOf(',');
    const int c2 = c1 < 0 ? -1 : v.indexOf(',', c1 + 1);
    const int c3 = c2 < 0 ? -1 : v.indexOf(',', c2 + 1);
    const int c4 = c3 < 0 ? -1 : v.indexOf(',', c3 + 1);
    if (c4 < 0) continue;
    const uint8_t mode = ModStore::modeFromName(v.substring(0, c1).c_str());
    if (mode == ModStore::OFF) continue;
    const float mn = v.substring(c1 + 1, c2).toFloat();
    const float mx = v.substring(c2 + 1, c3).toFloat();
    const float rate = v.substring(c3 + 1, c4).toFloat();
    const float step = v.substring(c4 + 1).toFloat();
    mods.set(key.c_str(), mode, mn, mx, rate, step, params.num(key.c_str(), mn));
  }
}

void savePatternMods(int idx) {
  String blob;
  for (int i = 0; i < mods.count(); i++) {
    const ModStore::Entry* e = mods.at(i);
    blob += e->key;
    blob += '=';
    blob += ModStore::modeName(e->mode);
    blob += ',' + String(e->minV, 4) + ',' + String(e->maxV, 4) + ',' +
            String(e->rate, 4) + ',' + String(e->step, 4) + '\n';
  }
  prefs.begin("cube", false);
  if (blob.length())
    prefs.putString(modKeyFor(idx).c_str(), blob);
  else
    prefs.remove(modKeyFor(idx).c_str());
  prefs.end();
}

void setPatternByIndex(int i) {
  activePatternIdx = ((i % kPatternCount) + kPatternCount) % kPatternCount;
  activePattern = kPatterns[activePatternIdx];
  patternStartMs = millis();
  lastT = 0;
  ctx.t = 0;
  ctx.dt = 0;
  params.clear();
  loadPatternParams(activePatternIdx);
  loadPatternMods(activePatternIdx);
  if (activePattern->init) activePattern->init(ctx);
  settings.patternId = activePattern->id;
  saveSetting("pattern", settings.patternId);
  Serial.printf("[pattern] %s\n", activePattern->id);
}

void setPatternById(const String& id) {
  for (int i = 0; i < kPatternCount; i++) {
    if (id == kPatterns[i]->id) {
      setPatternByIndex(i);
      return;
    }
  }
}

// ---- presets ------------------------------------------------------------------
//
// Snapshot the live pattern + its effective params into a named preset file,
// and load one back. Param types mirror effectiveParam/applyParamFromString:
// type 1 = bool, 2/3/4 = enum/palette/string, everything else (0 num, 5 color)
// = number.

void savePresetSnapshot(const String& name, int priority, float dwellSec) {
  JsonDocument doc;
  doc["name"] = name;
  doc["pattern"] = settings.patternId;
  doc["priority"] = priority;
  doc["dwellSec"] = dwellSec;
  JsonObject p = doc["params"].to<JsonObject>();
  const PatternSpecs* ps = specsFor(settings.patternId.c_str());
  if (ps) {
    for (int i = 0; i < ps->count; i++) {
      const ParamSpec& sp = ps->specs[i];
      switch (sp.type) {
        case 1: p[sp.key] = params.boolean(sp.key, sp.defNum != 0); break;
        case 2:
        case 3:
        case 4: p[sp.key] = params.str(sp.key, sp.defStr); break;
        default: p[sp.key] = params.num(sp.key, sp.defNum); break;
      }
    }
  }
  // Carry any active per-param modulation with the preset.
  if (mods.count() > 0) {
    JsonObject m = doc["mods"].to<JsonObject>();
    for (int i = 0; i < mods.count(); i++) {
      const ModStore::Entry* e = mods.at(i);
      JsonObject o = m[e->key].to<JsonObject>();
      o["mode"] = ModStore::modeName(e->mode);
      o["min"] = e->minV;
      o["max"] = e->maxV;
      o["rate"] = e->rate;
      o["step"] = e->step;
    }
  }
  presetWrite(name, doc);
}

bool loadPresetByName(const String& name) {
  JsonDocument doc;
  if (!presetRead(name, doc)) return false;
  const char* pat = doc["pattern"] | "";
  if (pat[0]) setPatternById(String(pat));  // clears params, loads NVS defaults, inits
  JsonObject p = doc["params"].as<JsonObject>();
  const PatternSpecs* ps = specsFor(settings.patternId.c_str());
  if (ps && !p.isNull()) {
    for (int i = 0; i < ps->count; i++) {
      const ParamSpec& sp = ps->specs[i];
      if (p[sp.key].isNull()) continue;
      switch (sp.type) {
        case 1: params.setBool(sp.key, p[sp.key].as<bool>()); break;
        case 2:
        case 3:
        case 4: params.setStr(sp.key, p[sp.key].as<const char*>()); break;
        default: params.setNum(sp.key, p[sp.key].as<float>()); break;
      }
    }
  }
  // If the preset carries modulation, it replaces whatever setPatternById
  // loaded from NVS. Only numeric params can be modulated.
  JsonObject m = doc["mods"].as<JsonObject>();
  if (!m.isNull()) {
    mods.clear();
    for (JsonPair kv : m) {
      const ParamSpec* mp = nullptr;
      if (ps)
        for (int i = 0; i < ps->count; i++)
          if (!strcmp(ps->specs[i].key, kv.key().c_str())) { mp = &ps->specs[i]; break; }
      if (!mp || mp->type != 0) continue;
      JsonObject o = kv.value().as<JsonObject>();
      const uint8_t mode = ModStore::modeFromName(o["mode"] | "off");
      if (mode == ModStore::OFF) continue;
      mods.set(kv.key().c_str(), mode, o["min"] | mp->minV, o["max"] | mp->maxV,
               o["rate"] | 1.0f, o["step"] | mp->stepV,
               params.num(kv.key().c_str(), mp->defNum));
    }
  }
  if (activePattern && activePattern->init) activePattern->init(ctx);
  return true;
}

// ---- playlist cycling (cube-eq5.2 / cube-eq5.3) -------------------------------
//
// Auto-cycles through the saved presets, each shown for its own dwellSec. The
// order is the preset store's listing order. Shuffle picks the next preset by
// weighted random on priority (weight = priority, linear: a priority-5 preset
// is 5x as likely as priority-1); priority 0 is EXCLUDED from shuffle rotation
// entirely (kept/saved but never auto-played). In-order cycling walks the
// whole list including priority-0 presets — priority only affects shuffle.
// RNG is esp_random() (hardware; no seeding, on-device visual choice).

struct PlaylistState {
  bool enabled = false;
  bool shuffle = false;
  String current;         // name of the preset currently showing
  uint32_t startedMs = 0;  // millis() when it began
  float dwellSec = 20;     // dwell of the current preset
};
PlaylistState playlist;
// Set while the playlist engine itself loads a preset, so the auto-pause hook
// (which fires on manual pattern/param/preset changes) doesn't stop the cycle.
bool playlistLoading = false;

// Manual pattern/param/preset change stops the cycle so tinkering isn't stomped.
void pausePlaylistForManual() {
  if (!playlistLoading) playlist.enabled = false;
}

void playlistLoadIndex(const PresetMeta* metas, int i) {
  playlistLoading = true;
  loadPresetByName(metas[i].name);
  playlistLoading = false;
  playlist.current = metas[i].name;
  playlist.dwellSec = metas[i].dwellSec > 0 ? metas[i].dwellSec : 20.0f;
  playlist.startedMs = millis();
}

// Shuffle pick: weighted by priority, priority-0 excluded, avoiding an
// immediate repeat when another eligible preset exists. Returns -1 if nothing
// is eligible (every preset is priority 0).
int playlistPickWeighted(const PresetMeta* metas, int n, const String& avoid) {
  int elExAvoid = 0;
  for (int i = 0; i < n; i++)
    if (metas[i].priority > 0 && metas[i].name != avoid) elExAvoid++;
  const bool skipAvoid = elExAvoid > 0;  // only avoid repeat when alternatives exist
  int total = 0;
  for (int i = 0; i < n; i++) {
    if (metas[i].priority <= 0) continue;
    if (skipAvoid && metas[i].name == avoid) continue;
    total += metas[i].priority;
  }
  if (total <= 0) return -1;
  int r = (int)(esp_random() % (uint32_t)total);
  for (int i = 0; i < n; i++) {
    if (metas[i].priority <= 0) continue;
    if (skipAvoid && metas[i].name == avoid) continue;
    r -= metas[i].priority;
    if (r < 0) return i;
  }
  return -1;
}

// Advance to the next preset. dir = +1 next / -1 prev (in-order only; shuffle
// always re-picks at random regardless of dir sign). Disables the playlist if
// the store is empty or nothing is eligible for shuffle.
void playlistAdvance(int dir) {
  PresetMeta metas[kMaxPresets];
  const int n = presetList(metas, kMaxPresets);
  if (n == 0) {
    playlist.enabled = false;
    return;
  }
  int pick;
  if (playlist.shuffle) {
    pick = playlistPickWeighted(metas, n, playlist.current);
    if (pick < 0) {  // all priority 0 -> nothing to auto-play
      playlist.enabled = false;
      return;
    }
  } else {
    int cur = -1;
    for (int i = 0; i < n; i++)
      if (metas[i].name == playlist.current) { cur = i; break; }
    pick = cur < 0 ? 0 : (((cur + dir) % n) + n) % n;
  }
  playlistLoadIndex(metas, pick);
}

// Called every loop: when a preset's dwell elapses, roll to the next.
void playlistTick() {
  if (!playlist.enabled) return;
  if ((millis() - playlist.startedMs) / 1000.0f < playlist.dwellSec) return;
  playlistAdvance(+1);
}

// ---- DNRGB live override ------------------------------------------------------

WiFiUDP udp;
constexpr uint16_t kRealtimePort = 21324;
constexpr int kUdpBufSize = 4 + 489 * 3;
uint8_t udpBuf[kUdpBufSize];
uint8_t liveFrame[NUM_LEDS * 3];
uint32_t liveUntilMs = 0;

void handleRealtime() {
  int size;
  while ((size = udp.parsePacket()) > 0) {
    if (size > kUdpBufSize) size = kUdpBufSize;
    const int n = udp.read(udpBuf, size);
    if (n < 4 || udpBuf[0] != 0x04) continue;  // DNRGB only
    const uint8_t timeoutS = udpBuf[1];
    const int start = (udpBuf[2] << 8) | udpBuf[3];
    const int count = (n - 4) / 3;
    if (start < 0 || start >= NUM_LEDS) continue;
    const int copy = min(count, NUM_LEDS - start);
    memcpy(liveFrame + start * 3, udpBuf + 4, copy * 3);
    liveUntilMs = millis() + (uint32_t)timeoutS * 1000;
  }
}

// ---- function button ----------------------------------------------------------

void handleButton() {
#if CUBE_BUTTON_PIN >= 0
  static uint32_t lastEdgeMs = 0;
  static bool lastState = true;
  const bool state = digitalRead(CUBE_BUTTON_PIN);
  const uint32_t now = millis();
  if (state != lastState && now - lastEdgeMs > 50) {
    lastEdgeMs = now;
    lastState = state;
    if (!state) setPatternByIndex(activePatternIdx + 1);  // press = next pattern
  }
#endif
}

// ---- WiFi supervisor ------------------------------------------------------------
//
// Boot-time-only fallback proved too fragile (a mistyped password left the
// cube dark until a power cycle landed just right). This state machine
// guarantees reachability at all times:
//  - configured SSID -> try to join for 20s -> fall back to the AP on failure
//  - joined but the connection drops for >10s -> AP comes back up
//  - while in AP fallback with an SSID configured -> retry the STA join every
//    2 minutes (in AP_STA, so the hotspot stays up during retries)
//  - a successful late join drops the AP and goes clean STA

enum class NetState { StaConnecting, StaOnline, ApFallback };
NetState netState = NetState::ApFallback;
uint32_t netStampMs = 0;
uint32_t lastStaRetryMs = 0;
uint32_t staLostMs = 0;

// Credential test (from the /wifi page): trial join in AP_STA so the page
// stays reachable. Result is polled via GET /api/wifitest.
bool wifiTestActive = false;
uint32_t wifiTestStartMs = 0;
String wifiTestResult = "idle";  // idle | testing | ok | fail
String wifiTestIp = "";

void startNetServices();

void startAp() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("cube-light", settings.apPass.c_str());
  netState = NetState::ApFallback;
  netStampMs = millis();
  lastStaRetryMs = millis();
  Serial.printf("[net] ap up, ip=%s\n", WiFi.softAPIP().toString().c_str());
  startNetServices();
}

void netBegin() {
  if (settings.wifiSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("cube");
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
    netState = NetState::StaConnecting;
    netStampMs = millis();
    Serial.printf("[net] joining %s...\n", settings.wifiSsid.c_str());
  } else {
    startAp();
  }
}

void netTick() {
  if (wifiTestActive) {  // supervisor paused during a credential test
    if (WiFi.status() == WL_CONNECTED) {
      wifiTestResult = "ok";
      wifiTestIp = WiFi.localIP().toString();
      wifiTestActive = false;
      Serial.printf("[net] test ok, ip=%s\n", wifiTestIp.c_str());
    } else if (millis() - wifiTestStartMs > 15000 ||
               WiFi.status() == WL_CONNECT_FAILED) {
      wifiTestResult = "fail";
      wifiTestActive = false;
      WiFi.disconnect(false);
      Serial.println("[net] test failed");
    }
    return;
  }

  const uint32_t now = millis();
  switch (netState) {
    case NetState::StaConnecting:
      if (WiFi.status() == WL_CONNECTED) {
        netState = NetState::StaOnline;
        staLostMs = 0;
        Serial.printf("[net] sta ip=%s\n", WiFi.localIP().toString().c_str());
        startNetServices();
      } else if (now - netStampMs > 20000) {
        Serial.println("[net] join timed out; falling back to AP");
        startAp();
      }
      break;
    case NetState::StaOnline:
      if (WiFi.status() == WL_CONNECTED) {
        staLostMs = 0;
      } else if (staLostMs == 0) {
        staLostMs = now;
      } else if (now - staLostMs > 10000) {
        Serial.println("[net] sta lost; AP fallback (will keep retrying)");
        WiFi.mode(WIFI_AP_STA);
        WiFi.softAP("cube-light", settings.apPass.c_str());
        WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
        netState = NetState::ApFallback;
        lastStaRetryMs = now;
      }
      break;
    case NetState::ApFallback:
      if (settings.wifiSsid.length() > 0) {
        if (WiFi.status() == WL_CONNECTED) {
          Serial.printf("[net] late join ok, ip=%s; dropping AP\n",
                        WiFi.localIP().toString().c_str());
          WiFi.softAPdisconnect(true);
          WiFi.mode(WIFI_STA);
          netState = NetState::StaOnline;
          staLostMs = 0;
        } else if (now - lastStaRetryMs > 120000) {
          lastStaRetryMs = now;
          Serial.println("[net] retrying sta join (AP stays up)");
          WiFi.mode(WIFI_AP_STA);
          WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
        }
      }
      break;
  }
}

// ---- web UI -------------------------------------------------------------------

WebServer server(80);

// Optional console password (HTTP Basic auth, username "cube"). Gates the
// ADMIN tier only: hardware, network, calibration, and anything persistent.
// The guest tier (pattern picking, live params, brightness, game pad) stays
// open so the cube can be handed to a crowd.
bool authed() {
  if (settings.uiPass.length() == 0) return true;
  if (server.authenticate("cube", settings.uiPass.c_str())) return true;
  server.requestAuthentication();
  return false;
}

// A "guest" is anyone connected to the cube's own SoftAP (typically the
// 192.168.4.x subnet) — a party-goer with a phone. Clients reaching us over
// STA / the home LAN are the owner. We decide by comparing the requester's
// IP against the SoftAP network, so it needs no password and can't be
// spoofed by the UI. When no AP is up (clean STA), there are no guests.
bool isGuestRequest() {
  if (WiFi.getMode() == WIFI_STA) return false;
  const IPAddress ap = WiFi.softAPIP();
  const IPAddress cl = server.client().remoteIP();
  return cl[0] == ap[0] && cl[1] == ap[1] && cl[2] == ap[2];
}

// Reject owner-only actions from guests with a friendly 403. Returns true if
// the request was blocked (caller should return immediately).
bool guestBlocked() {
  if (isGuestRequest()) {
    server.send(403, "text/plain",
                "That's owner-only. Guests can play with patterns and knobs — "
                "and hit the reset button — but can't change saved settings.");
    return true;
  }
  return false;
}

void handleStatus() {
  String json = "{\"pattern\":\"" + settings.patternId + "\",\"patterns\":[";
  for (int i = 0; i < kPatternCount; i++) {
    if (i) json += ',';
    json += '"';
    json += kPatterns[i]->id;
    json += '"';
  }
  json += "],\"brightness\":" + String(settings.brightness, 2);
  json += ",\"supplyMA\":" + String(settings.supplyMA);
  json += ",\"colorOrder\":\"" + settings.colorOrder + "\"";
  json += ",\"ip\":\"" +
          (WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString()
                                     : WiFi.localIP().toString()) +
          "\"";
  json += ",\"rssi\":" + String(WiFi.RSSI());
  json += ",\"fps\":" + String(CUBE_FPS);
  json += ",\"uptimeS\":" + String(millis() / 1000);
  json += ",\"micOn\":" + String(settings.micEnabled ? "true" : "false");
  json += ",\"guest\":" + String(isGuestRequest() ? "true" : "false");
  json += ",\"live\":" + String(millis() < liveUntilMs ? "true" : "false");
  json += ",\"up\":\"" + settings.upAxis + "\"";
  json += ",\"ledPin\":" + String(settings.ledPin) + ",\"ledPin2\":" + String(settings.ledPin2) +
          ",\"ledSplit\":" + String(settings.ledSplit);
  json += ",\"layout\":{\"flipX\":" + String(settings.layout.flipX ? "true" : "false") +
          ",\"flipY\":" + String(settings.layout.flipY ? "true" : "false") +
          ",\"flipZ\":" + String(settings.layout.flipZ ? "true" : "false") +
          ",\"ledOffset\":" + String(settings.layout.ledOffset) + "}";
  float dwellRem = 0;
  if (playlist.enabled) {
    dwellRem = playlist.dwellSec - (millis() - playlist.startedMs) / 1000.0f;
    if (dwellRem < 0) dwellRem = 0;
  }
  String plCur = playlist.current;
  plCur.replace("\\", "\\\\");
  plCur.replace("\"", "\\\"");
  json += ",\"playlist\":{\"enabled\":" + String(playlist.enabled ? "true" : "false") +
          ",\"shuffle\":" + String(playlist.shuffle ? "true" : "false") +
          ",\"current\":\"" + plCur + "\",\"dwellRemainingSec\":" + String(dwellRem, 0) + "}";
  json += ",\"version\":\"" CUBE_VERSION "\"}";
  server.send(200, "application/json", json);
}

void setupWebServer();
bool servicesStarted = false;

// mDNS, OTA, the realtime UDP listener, and the HTTP console. Must only run
// once a network interface exists; starting them during the STA join
// corrupts the WiFi blob's management-frame callbacks (InstructionFetchError
// in sta_recv_mgmt) and crash-loops the chip.
void startNetServices() {
  if (servicesStarted) return;
  servicesStarted = true;
  MDNS.begin("cube");  // http://cube.local/
  ArduinoOTA.setHostname("cube");
  ArduinoOTA.setPassword(settings.apPass.c_str());
  ArduinoOTA.begin();
  udp.begin(kRealtimePort);
  setupWebServer();
  audioCaptureStart();
  audioCaptureReconfigure(settings.micLeft, settings.micSquelch);
  Serial.println("[net] services up (mdns/ota/udp/http/mic)");
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", kIndexHtml);
  });
  server.on("/api/status", HTTP_GET, []() {
    handleStatus();
  });
  server.on("/api/pattern", HTTP_POST, []() {
    pausePlaylistForManual();  // manual pick stops the cycle
    setPatternById(server.arg("id"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/brightness", HTTP_POST, []() {
    settings.brightness = constrain(server.arg("v").toFloat(), 0.0f, 1.0f);
    saveSetting("bright", settings.brightness);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/supply", HTTP_POST, []() {
    if (!authed()) return;
    settings.supplyMA = (uint32_t)server.arg("ma").toInt();
    saveSetting("supply", settings.supplyMA);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/order", HTTP_POST, []() {
    if (!authed()) return;
    settings.colorOrder = server.arg("v");
    applyColorOrder(settings.colorOrder);
    saveSetting("order", settings.colorOrder);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/scan", HTTP_GET, []() {
    if (!authed()) return;
    // Synchronous scan (~2s; patterns pause one beat — fine on a settings
    // page). Needs the STA interface alongside a running AP.
    if (WiFi.getMode() == WIFI_AP) WiFi.mode(WIFI_AP_STA);
    const int n = WiFi.scanNetworks();
    String json = "[";
    int emitted = 0;
    for (int i = 0; i < n && emitted < 15; i++) {
      const String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      bool dup = false;  // keep the strongest instance of each SSID
      for (int j = 0; j < i; j++) {
        if (WiFi.SSID(j) == ssid) { dup = true; break; }
      }
      if (dup) continue;
      if (emitted) json += ',';
      String esc = ssid;
      esc.replace("\\", "\\\\");
      esc.replace("\"", "\\\"");
      json += "{\"ssid\":\"" + esc + "\",\"rssi\":" + String(WiFi.RSSI(i)) +
              ",\"open\":" + (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "true" : "false") + "}";
      emitted++;
    }
    json += "]";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
  });
  server.on("/api/wifitest", HTTP_POST, []() {
    if (!authed()) return;
    // Trial join in AP_STA so the hotspot (and this page) survive the test.
    WiFi.mode(WIFI_AP_STA);
    WiFi.begin(server.arg("ssid").c_str(), server.arg("pass").c_str());
    wifiTestActive = true;
    wifiTestStartMs = millis();
    wifiTestResult = "testing";
    wifiTestIp = "";
    server.send(200, "text/plain", "started");
  });
  server.on("/api/wifitest", HTTP_GET, []() {
    if (!authed()) return;
    server.send(200, "application/json",
                "{\"state\":\"" + wifiTestResult + "\",\"ip\":\"" + wifiTestIp + "\"}");
  });
  // Param specs + current values for a pattern (default: the active one).
  server.on("/api/params", HTTP_GET, []() {
    String id = server.hasArg("id") ? server.arg("id") : settings.patternId;
    const PatternSpecs* ps = specsFor(id.c_str());
    if (!ps) {
      server.send(404, "application/json", "{\"error\":\"unknown pattern\"}");
      return;
    }
    const bool isActive = id == settings.patternId;
    String json = "{\"id\":\"" + id + "\",\"active\":" + (isActive ? "true" : "false") +
                  ",\"palettes\":[";
    for (int i = 0; i < kPaletteNameCount; i++) {
      if (i) json += ',';
      json += '"';
      json += kPaletteNames[i];
      json += '"';
    }
    json += "],\"specs\":[";
    for (int i = 0; i < ps->count; i++) {
      const ParamSpec& sp = ps->specs[i];
      if (i) json += ',';
      json += "{\"key\":\"" + String(sp.key) + "\",\"label\":\"" + String(sp.label) +
              "\",\"type\":" + String(sp.type) + ",\"min\":" + String(sp.minV, 3) +
              ",\"max\":" + String(sp.maxV, 3) + ",\"step\":" + String(sp.stepV, 3) +
              ",\"options\":\"" + String(sp.options) + "\",\"value\":\"" +
              (isActive ? effectiveParam(sp)
                        : (sp.type == 0 ? String(sp.defNum, 3)
                           : sp.type == 1 ? String(sp.defNum != 0 ? "1" : "0")
                                          : String(sp.defStr))) +
              "\"";
      // Active-pattern numeric params carry their modulation config, if any.
      const ModStore::Entry* me = isActive ? mods.find(sp.key) : nullptr;
      if (me)
        json += ",\"mod\":{\"mode\":\"" + String(ModStore::modeName(me->mode)) +
                "\",\"min\":" + String(me->minV, 3) + ",\"max\":" + String(me->maxV, 3) +
                ",\"rate\":" + String(me->rate, 3) + ",\"step\":" + String(me->step, 3) + "}";
      json += "}";
    }
    json += "]}";
    server.send(200, "application/json", json);
  });
  // Persist the active pattern's current params as its power-on defaults.
  server.on("/api/params/save", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    savePatternParams(activePatternIdx);
    savePatternMods(activePatternIdx);
    server.send(200, "text/plain", "ok");
  });
  // Guest-tier master toggle for audio reactivity: off = patterns see
  // silence (capture keeps running so the meter still works for admins).
  server.on("/api/mic", HTTP_POST, []() {
    settings.micEnabled = server.arg("on") != "0";
    prefs.begin("cube", false);
    prefs.putBool("micen", settings.micEnabled);
    prefs.end();
    server.send(200, "text/plain", "ok");
  });
  // Guest-tier reset: discard live tweaks, back to the saved power-on
  // defaults (the "un-mess" button).
  server.on("/api/params/reset", HTTP_POST, []() {
    params.clear();
    loadPatternParams(activePatternIdx);
    loadPatternMods(activePatternIdx);
    if (activePattern->init) activePattern->init(ctx);
    server.send(200, "text/plain", "ok");
  });
  // Admin-tier factory reset: also delete the saved defaults.
  server.on("/api/params/factory", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    prefs.begin("cube", false);
    prefs.remove(paramsKeyFor(activePatternIdx).c_str());
    prefs.remove(modKeyFor(activePatternIdx).c_str());
    prefs.end();
    params.clear();
    mods.clear();
    if (activePattern->init) activePattern->init(ctx);
    server.send(200, "text/plain", "ok");
  });
  // ---- presets ----
  // List saved presets (metadata only; params omitted). Open to guests.
  server.on("/api/presets", HTTP_GET, []() {
    PresetMeta metas[kMaxPresets];
    const int n = presetList(metas, kMaxPresets);
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i) json += ',';
      String nm = metas[i].name;
      nm.replace("\\", "\\\\");
      nm.replace("\"", "\\\"");
      json += "{\"name\":\"" + nm + "\",\"pattern\":\"" + metas[i].pattern +
              "\",\"priority\":" + String(metas[i].priority) +
              ",\"dwellSec\":" + String(metas[i].dwellSec, 0) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });
  // Snapshot the live pattern + params as a named preset. Owner-only.
  server.on("/api/presets/save", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    const String name = server.arg("name");
    if (name.length() == 0) {
      server.send(400, "text/plain", "name required");
      return;
    }
    const int priority =
        server.hasArg("priority") ? constrain((int)server.arg("priority").toInt(), 0, 5) : 3;
    const float dwell = server.hasArg("dwellSec") ? server.arg("dwellSec").toFloat() : 20.0f;
    savePresetSnapshot(name, priority, dwell);
    server.send(200, "text/plain", "ok");
  });
  // Apply a preset (changes the live pattern) — allowed for guests.
  server.on("/api/presets/load", HTTP_POST, []() {
    pausePlaylistForManual();  // loading a single preset stops the cycle
    if (!loadPresetByName(server.arg("name"))) {
      server.send(404, "text/plain", "no such preset");
      return;
    }
    server.send(200, "text/plain", "ok");
  });
  // Delete a preset. Owner-only.
  server.on("/api/presets/delete", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    presetDelete(server.arg("name"));
    server.send(200, "text/plain", "ok");
  });
  // Edit just a preset's playlist metadata (dwellSec/priority) without
  // re-snapshotting its params. Read-modify-write. Owner-only.
  server.on("/api/presets/meta", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    const String name = server.arg("name");
    JsonDocument doc;
    if (!presetRead(name, doc)) {
      server.send(404, "text/plain", "no such preset");
      return;
    }
    if (server.hasArg("priority"))
      doc["priority"] = constrain((int)server.arg("priority").toInt(), 0, 5);
    if (server.hasArg("dwellSec")) doc["dwellSec"] = server.arg("dwellSec").toFloat();
    presetWrite(name, doc);
    // Reflect a new dwell on the live preset immediately.
    if (playlist.enabled && playlist.current == name && server.hasArg("dwellSec"))
      playlist.dwellSec = server.arg("dwellSec").toFloat();
    server.send(200, "text/plain", "ok");
  });
  // Download presets as a JSON file (Content-Disposition triggers a browser
  // save). ?name=<n> exports one preset object; no arg exports the whole
  // library as a bundle {"version":1,"presets":[<preset objects>]}. The bundle
  // shape is exactly what /api/presets/import consumes, so the round-trip is
  // download -> hand-edit / AI-edit -> re-import. Owner-only.
  server.on("/api/presets/export", HTTP_GET, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (server.hasArg("name")) {
      JsonDocument doc;
      if (!presetRead(server.arg("name"), doc)) {
        server.send(404, "text/plain", "no such preset");
        return;
      }
      String out;
      serializeJson(doc, out);
      server.sendHeader("Content-Disposition",
                        "attachment; filename=\"" + presetSlug(server.arg("name")) + ".json\"");
      server.send(200, "application/json", out);
      return;
    }
    PresetMeta metas[kMaxPresets];
    const int n = presetList(metas, kMaxPresets);
    String out = "{\"version\":1,\"presets\":[";
    bool first = true;
    for (int i = 0; i < n; i++) {
      JsonDocument doc;
      if (!presetRead(metas[i].name, doc)) continue;
      String one;
      serializeJson(doc, one);
      if (!first) out += ',';
      out += one;
      first = false;
    }
    out += "]}";
    server.sendHeader("Content-Disposition", "attachment; filename=\"cube-presets.json\"");
    server.send(200, "application/json", out);
  });
  // Import presets from a JSON body (raw request body, read via arg("plain")).
  // Accepts a bundle {"version":..,"presets":[..]}, a bare array of preset
  // objects, or a single preset object. Each preset OVERWRITES any existing one
  // with the same slug (import replaces, per spec). Entries missing name or
  // pattern are skipped; new presets beyond kMaxPresets are skipped once the
  // cap is hit (overwrites of existing presets are always allowed). Returns
  // {"imported":n,"overwritten":n,"skipped":n}. Owner-only.
  server.on("/api/presets/import", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain"))) {
      server.send(400, "text/plain", "invalid json");
      return;
    }
    PresetMeta metas[kMaxPresets];
    int count = presetList(metas, kMaxPresets);
    int imported = 0, overwritten = 0, skipped = 0;
    auto importOne = [&](JsonObject o) {
      const char* nm = o["name"] | "";
      const char* pat = o["pattern"] | "";
      if (!nm[0] || !pat[0]) { skipped++; return; }
      const String name = nm;
      JsonDocument tmp;
      const bool exists = presetRead(name, tmp);
      if (!exists && count >= kMaxPresets) { skipped++; return; }
      JsonDocument out;
      out.set(o);  // deep copy of this preset object
      if (out["priority"].isNull()) out["priority"] = 3;
      if (out["dwellSec"].isNull()) out["dwellSec"] = 20.0f;
      presetWrite(name, out);
      if (exists) overwritten++;
      else { imported++; count++; }
    };
    if (doc["presets"].is<JsonArray>()) {
      for (JsonObject o : doc["presets"].as<JsonArray>()) importOne(o);
    } else if (doc.is<JsonArray>()) {
      for (JsonObject o : doc.as<JsonArray>()) importOne(o);
    } else if (doc.is<JsonObject>()) {
      importOne(doc.as<JsonObject>());
    } else {
      server.send(400, "text/plain", "invalid json");
      return;
    }
    server.send(200, "application/json",
                "{\"imported\":" + String(imported) + ",\"overwritten\":" +
                    String(overwritten) + ",\"skipped\":" + String(skipped) + "}");
  });
  // Rename a preset's display name. Writes the preset under the new slug and
  // deletes the old file if the slug changed. If ?to already exists (different
  // slug), it is OVERWRITTEN — same replace semantics as import. Owner-only.
  server.on("/api/presets/rename", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    const String from = server.arg("from");
    const String to = server.arg("to");
    if (from.length() == 0 || to.length() == 0) {
      server.send(400, "text/plain", "from and to required");
      return;
    }
    JsonDocument doc;
    if (!presetRead(from, doc)) {
      server.send(404, "text/plain", "no such preset");
      return;
    }
    doc["name"] = to;
    presetWrite(to, doc);
    if (presetSlug(from) != presetSlug(to)) presetDelete(from);
    if (playlist.current == from) playlist.current = to;
    server.send(200, "text/plain", "ok");
  });
  // ---- playlist cycling ----
  // Configure/toggle the cycle. Owner-only, EXCEPT a guest may pause
  // (enabled=0) so a party-goer can stop the rotation on a pattern they like.
  server.on("/api/playlist", HTTP_POST, []() {
    if (isGuestRequest()) {
      if (server.arg("enabled") == "0") {
        playlist.enabled = false;
        server.send(200, "text/plain", "ok");
      } else {
        guestBlocked();  // sends the 403
      }
      return;
    }
    if (!authed()) return;
    if (server.hasArg("shuffle")) playlist.shuffle = server.arg("shuffle") == "1";
    if (server.hasArg("enabled")) {
      const bool en = server.arg("enabled") == "1";
      if (en && !playlist.enabled) {
        playlist.enabled = true;
        playlist.current = "";
        playlistAdvance(+1);  // load the first preset now
      } else {
        playlist.enabled = en;
      }
    }
    server.send(200, "text/plain", "ok");
  });
  // Manual skip (owner-only). Enables the cycle if it was off.
  server.on("/api/playlist/next", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (!playlist.enabled) { playlist.enabled = true; }
    playlistAdvance(+1);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/playlist/prev", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (!playlist.enabled) { playlist.enabled = true; }
    playlistAdvance(-1);
    server.send(200, "text/plain", "ok");
  });
  // Runtime LED hardware config: pins + single/dual split. Applies live
  // (strips are rebuilt) and persists.
  server.on("/api/ledcfg", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    settings.ledPin = server.arg("pin").toInt();
    settings.ledPin2 = server.hasArg("pin2") ? server.arg("pin2").toInt() : -1;
    settings.ledSplit = server.hasArg("split") ? server.arg("split").toInt() : NUM_LEDS;
    saveHardware();
    initStrips();
    server.send(200, "text/plain", "ok");
  });
  // Mic diagnostics: live frame + raw capture stats.
  server.on("/api/audio", HTTP_GET, []() {
    AudioFrame af;
    audioCaptureRead(af);
    AudioStats st;
    audioCaptureStats(st);
    String json = "{\"level\":" + String(af.level, 3) + ",\"beat\":" + String(af.beat, 3) +
                  ",\"bands\":[";
    for (int i = 0; i < AUDIO_BANDS; i++) {
      if (i) json += ',';
      json += String(af.bands[i], 3);
    }
    json += "],\"frames\":" + String(st.frames) + ",\"rms\":" + String(st.lastRms, 1) +
            ",\"dc\":" + String(st.lastDc, 1) + ",\"rawMin\":" + String(st.rawMin) +
            ",\"rawMax\":" + String(st.rawMax) +
            ",\"channel\":\"" + String(settings.micLeft ? "left" : "right") + "\"" +
            ",\"squelch\":" + String(settings.micSquelch, 1) + "}";
    server.send(200, "application/json", json);
  });
  server.on("/api/miccfg", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (server.hasArg("ch")) settings.micLeft = server.arg("ch") == "left";
    if (server.hasArg("squelch")) settings.micSquelch = server.arg("squelch").toFloat();
    saveHardware();
    audioCaptureReconfigure(settings.micLeft, settings.micSquelch);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/up", HTTP_POST, []() {
    if (!authed()) return;
    settings.upAxis = server.arg("v");
    applyGeometry();
    saveLayoutAndUp();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/layout", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    settings.layout.flipX = server.arg("fx") == "1";
    settings.layout.flipY = server.arg("fy") == "1";
    settings.layout.flipZ = server.arg("fz") == "1";
    settings.layout.ledOffset = server.arg("off").toInt();
    applyGeometry();
    saveLayoutAndUp();
    server.send(200, "text/plain", "ok");
  });
  // Set a live pattern parameter (not persisted). Used by the calibration
  // page to steer lit-pixel, and handy for tweaking any pattern.
  server.on("/api/param", HTTP_POST, []() {
    pausePlaylistForManual();  // manual tweak stops the cycle
    const String key = server.arg("key");
    const String v = server.arg("v");
    const String type = server.arg("type");
    if (type == "str") params.setStr(key.c_str(), v.c_str());
    else if (type == "bool") params.setBool(key.c_str(), v == "true" || v == "1");
    else params.setNum(key.c_str(), v.toFloat());
    server.send(200, "text/plain", "ok");
  });
  // Configure automatic modulation for one NUMERIC param of the active pattern
  // (cube-la3). mode=off clears it. Omitted min/max/rate/step default from the
  // param's spec. Owner-only. See cube_mod.h for the pingpong/walk math.
  server.on("/api/param/mod", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    pausePlaylistForManual();
    const String key = server.arg("key");
    if (key.length() == 0) {
      server.send(400, "text/plain", "key required");
      return;
    }
    const uint8_t mode = ModStore::modeFromName(server.arg("mode").c_str());
    if (mode == ModStore::OFF) {
      mods.remove(key.c_str());
      server.send(200, "text/plain", "ok");
      return;
    }
    // Modulation is numeric-only: find the spec and reject non-number params.
    const PatternSpecs* ps = specsFor(settings.patternId.c_str());
    const ParamSpec* sp = nullptr;
    if (ps)
      for (int i = 0; i < ps->count; i++)
        if (key == ps->specs[i].key) { sp = &ps->specs[i]; break; }
    if (!sp || sp->type != 0) {
      server.send(400, "text/plain", "not a numeric param");
      return;
    }
    float mn, mx, rate, step;
    ModStore::defaults(*sp, mode, mn, mx, rate, step);
    if (server.hasArg("min")) mn = server.arg("min").toFloat();
    if (server.hasArg("max")) mx = server.arg("max").toFloat();
    if (server.hasArg("rate")) rate = server.arg("rate").toFloat();
    if (server.hasArg("step")) step = server.arg("step").toFloat();
    mods.set(key.c_str(), mode, mn, mx, rate, step,
             params.num(key.c_str(), sp->defNum));
    server.send(200, "text/plain", "ok");
  });
  // Body: text lines "led,x,y,z". Returns candidates/suggestion as JSON.
  server.on("/api/calibrate/solve", HTTP_POST, []() {
    if (!authed()) return;
    static CalSample samples[64];
    int count = 0;
    const String body = server.arg("plain");
    int pos = 0;
    while (pos < (int)body.length() && count < 64) {
      int nl = body.indexOf('\n', pos);
      if (nl < 0) nl = body.length();
      int a, b, c, d;
      if (sscanf(body.substring(pos, nl).c_str(), "%d,%d,%d,%d", &a, &b, &c, &d) == 4 &&
          a >= 0 && a < NUM_LEDS && b >= 0 && b < CUBE_N && c >= 0 && c < CUBE_N &&
          d >= 0 && d < CUBE_N) {
        samples[count++] = {(uint16_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d};
      }
      pos = nl + 1;
    }
    const CalResult r = solveCalibration(samples, count);
    String json = "{\"samples\":" + String(count) + ",\"candidates\":[";
    for (int i = 0; i < r.candidateCount; i++) {
      if (i) json += ',';
      json += "{\"fx\":" + String(r.candidates[i].flipX ? 1 : 0) +
              ",\"fy\":" + String(r.candidates[i].flipY ? 1 : 0) +
              ",\"fz\":" + String(r.candidates[i].flipZ ? 1 : 0) +
              ",\"off\":" + String(r.candidates[i].ledOffset) + "}";
    }
    json += "],\"suggest\":" + String(r.suggestedNextLed);
    if (r.candidateCount == 0 && count > 0) {
      json += ",\"bestEffort\":{\"fx\":" + String(r.bestEffort.flipX ? 1 : 0) +
              ",\"fy\":" + String(r.bestEffort.flipY ? 1 : 0) +
              ",\"fz\":" + String(r.bestEffort.flipZ ? 1 : 0) +
              ",\"off\":" + String(r.bestEffort.ledOffset) +
              ",\"misses\":" + String(r.bestEffortMisses) + "}";
    }
    json += "}";
    server.send(200, "application/json", json);
  });
  server.on("/calibrate", HTTP_GET, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    server.send(200, "text/html", kCalibrateHtml);
  });
  server.on("/admin", HTTP_GET, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    server.send(200, "text/html", kAdminHtml);
  });
  server.on("/leds", HTTP_GET, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    server.send(200, "text/html", kLedsHtml);
  });
  // Game pad: intentionally NOT auth-gated so guests can play snake/pacman
  // without the console password. Input queueing is harmless.
  server.on("/snake", HTTP_GET, []() { server.send(200, "text/html", kSnakeHtml); });
  server.on("/api/game", HTTP_POST, []() {
    const int d = server.arg("dir").toInt();
    if (d < 0 || d > 5) {
      server.send(400, "text/plain", "bad dir");
      return;
    }
    if (settings.patternId == "snake-3d") {
      params.setStr("mode", "manual");  // grabbing the pad takes over from auto
      queueSnakeInput((SnakeDir)d);
      server.send(200, "text/plain", "ok");
    } else if (settings.patternId == "pacman-3d") {
      params.setStr("mode", "manual");
      queuePacmanInput((SnakeDir)d);
      server.send(200, "text/plain", "ok");
    } else {
      server.send(200, "text/plain", "switch the cube to Snake or Pac-Man first");
    }
  });
  server.on("/wifi", HTTP_GET, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    String page = kWifiHtml;
    page.replace("%SSID%", settings.wifiSsid);
    page.replace("%UIPASS%", settings.uiPass.length() ? "(unchanged)" : "(not set)");
    server.send(200, "text/html", page);
  });
  server.on("/wifi", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (server.hasArg("ssid")) saveSetting("ssid", server.arg("ssid"));
    if (server.arg("pass").length() > 0) saveSetting("pass", server.arg("pass"));
    if (server.arg("appass").length() >= 8) saveSetting("appass", server.arg("appass"));
    if (server.hasArg("clearui")) saveSetting("uipass", String(""));
    else if (server.arg("uipass").length() >= 4) saveSetting("uipass", server.arg("uipass"));
    server.send(200, "text/html",
                "<body style=\"font-family:system-ui;background:#0d0d10;color:#ddd\">"
                "Saved. Rebooting&hellip; The cube joins your network, or its "
                "hotspot returns within ~30s if that fails.</body>");
    delay(300);
    ESP.restart();
  });
  server.begin();
}

// ---- setup / loop ---------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  loadSettings();
  presetsBegin();  // mount LittleFS for preset storage
  applyColorOrder(settings.colorOrder);
  applyGeometry();
#if CUBE_BUTTON_PIN >= 0
  pinMode(CUBE_BUTTON_PIN, INPUT_PULLUP);
#endif
#if CUBE_RELAY_PIN >= 0
  pinMode(CUBE_RELAY_PIN, OUTPUT);
  digitalWrite(CUBE_RELAY_PIN, HIGH);  // power the LED string
#endif
  initStrips();

  // Non-blocking: netTick() in loop() drives join/fallback, so patterns
  // start immediately and the AP always comes back if the network is lost.
  // The mic and all network services start only after an interface is up —
  // initializing anything heavy while the join handshake is in flight
  // corrupts the WiFi stack and crash-loops the chip (found the hard way).
  netBegin();

  setPatternById(settings.patternId);
  if (!activePattern) setPatternByIndex(0);
}

void loop() {
  netTick();
  if (!servicesStarted) {
    // Nothing else runs until the network is up: rendering to the RMT
    // while the join handshake is in flight is part of the crash recipe.
    delay(2);
    return;
  }
  ArduinoOTA.handle();
  server.handleClient();
  handleRealtime();
  handleButton();
  playlistTick();

  static uint32_t nextFrameMs = 0;
  const uint32_t now = millis();
  if (now < nextFrameMs) return;
  nextFrameMs = now + 1000 / CUBE_FPS;

  if (now < liveUntilMs) {
    show(liveFrame);  // dev server (or any WLED sender) has the cube
    return;
  }

  if (settings.micEnabled) {
    audioCaptureRead(audio);
  } else {
    audio = AudioFrame{};  // reactivity off: patterns see silence
  }
  const float t = (now - patternStartMs) / 1000.0f;
  ctx.t = t;
  ctx.dt = t - lastT;
  lastT = t;
  mods.tick(params, now);  // drift any modulated params before render sees them
  activePattern->render(ctx);
  show(frame);
}
