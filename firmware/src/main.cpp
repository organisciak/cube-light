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
//  - ArduinoOTA (password-protected) for wireless reflashing — DISARMED until
//    the owner opens a 15-minute window from /admin (see armOta()).
//
// Not yet here: mic capture + FFT -> BeatDetector, per-pattern params over
// the API, /snake controller page, full React-app WS contract.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <NeoPixelBus.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>
#include <mbedtls/base64.h>

#include "audio_capture.h"
#include "cube_image.h"
#include "cube_calibration.h"
#include "cube_mod.h"
#include "cube_pacman.h"
#include "cube_palettes.h"
#include "cube_param_specs.h"
#include "cube_pattern.h"
#include "cube_presets.h"
#include "cube_snake.h"
#include "cube_power.h"
#include "ha_mqtt.h"
#include "web_ui.h"

#define CUBE_VERSION "0.5.0"

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
#ifndef CUBE_MDNS_NAME
#define CUBE_MDNS_NAME "cube"  // mDNS host — give a second cube its own name
#endif
// Optional baked-in first-boot WiFi, for OTA-only boards that must come up on
// the LAN right after their first flash (no USB rescue). Credentials stay out
// of git: pass at build time via
//   PLATFORMIO_BUILD_FLAGS='-DCUBE_WIFI_SSID=\"...\" -DCUBE_WIFI_PASS=\"...\"'
// NVS-saved credentials always win; these are only the empty-NVS defaults.
#ifndef CUBE_WIFI_SSID
#define CUBE_WIFI_SSID ""
#endif
#ifndef CUBE_WIFI_PASS
#define CUBE_WIFI_PASS ""
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
  int ledSkip1;       // LEDs snipped off the START of output 1's chain
  int ledTrim1;       // LEDs snipped off the END of output 1's chain
  int ledSkip2;       // same, output 2
  int ledTrim2;
  bool micLeft;       // PDM channel format
  float micSquelch;   // raw RMS below this = silence
  bool micEnabled;    // master toggle: false = patterns see silence
  bool powerOn;       // HA light switch: false = dark (relay dropped if fitted)
  bool apGuests;      // true = hotspot clients are guests; false = the WPA2
                      // password is the only gate and AP clients are owners
  bool mqttEnabled;   // Home Assistant MQTT integration
  String mqttHost;
  uint16_t mqttPort;
  String mqttUser;
  String mqttPass;
};

Preferences prefs;
Settings settings;

void loadSettings() {
  prefs.begin("cube", true);
  settings.wifiSsid = prefs.getString("ssid", CUBE_WIFI_SSID);
  settings.wifiPass = prefs.getString("pass", CUBE_WIFI_PASS);
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
  settings.ledSkip1 = prefs.getInt("skip1", 0);
  settings.ledTrim1 = prefs.getInt("trim1", 0);
  settings.ledSkip2 = prefs.getInt("skip2", 0);
  settings.ledTrim2 = prefs.getInt("trim2", 0);
  settings.micLeft = prefs.getBool("micleft", false);
  settings.micSquelch = prefs.getFloat("micsq", 60.0f);
  settings.micEnabled = prefs.getBool("micen", true);
  settings.powerOn = prefs.getBool("power", true);
  settings.apGuests = prefs.getBool("apguests", true);
  settings.mqttEnabled = prefs.getBool("mqen", false);
  settings.mqttHost = prefs.getString("mqhost", "");
  settings.mqttPort = (uint16_t)prefs.getUInt("mqport", 1883);
  settings.mqttUser = prefs.getString("mquser", "");
  settings.mqttPass = prefs.getString("mqpass", "");
  prefs.end();
}

void saveHardware() {
  prefs.begin("cube", false);
  prefs.putInt("ledpin", settings.ledPin);
  prefs.putInt("ledpin2", settings.ledPin2);
  prefs.putInt("ledsplit", settings.ledSplit);
  prefs.putInt("skip1", settings.ledSkip1);
  prefs.putInt("trim1", settings.ledTrim1);
  prefs.putInt("skip2", settings.ledSkip2);
  prefs.putInt("trim2", settings.ledTrim2);
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
void saveSetting(const char* key, bool v) {
  // NVS skips the write when the stored value already matches, so calling
  // this redundantly (e.g. per button press) costs a read, not flash wear.
  prefs.begin("cube", false);
  prefs.putBool(key, v);
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

// Debug-only snip simulation, per output (NOT persisted — a reboot always
// clears it): pretend an intact chain lost simSkip[] LEDs at its start /
// simTrim[] at its end, so the ledSkip/ledTrim compensation can be rehearsed
// before (or without) cutting real LEDs. Set via POST /api/ledsim.
int simSkip[2] = {0, 0};
int simTrim[2] = {0, 0};

uint8_t colorPerm[3] = {0, 1, 2};  // perm[wireSlot] = source channel (0=R 1=G 2=B)

void applyColorOrder(const String& order) {
  for (int i = 0; i < 3 && i < (int)order.length(); i++) {
    colorPerm[i] = order[i] == 'G' ? 1 : order[i] == 'B' ? 2 : 0;
  }
}

// Push the logical frame to the strips, compensating for physically removed
// LEDs (burnt-out pixels snipped off a chain). If an output lost `ledSkip`
// LEDs at its start, data now enters at what used to be wire position
// base+skip — so data slot p carries logical LED (base + skip + p), every
// surviving LED keeps its calibrated position, and the snipped spots simply
// go dark. LEDs trimmed off the END need no shift; `ledTrim` just marks that
// tail absent. The simSkip/simTrim counters do the inverse on intact
// hardware: wire slot s displays what data slot (s - simSkip) would land
// there on a snipped chain, with the "missing" slots forced dark — so
// sim N + skip N should look identical to an untouched cube minus N LEDs.
void show(const uint8_t* rgb) {
  if (!strip1) return;
  const float limit = currentLimitScale(rgb, NUM_LEDS, CUBE_PER_LED_MA,
                                        CUBE_IDLE_MA_PER_LED, settings.supplyMA);
  const float k = limit * settings.brightness;
  const int base[2] = {0, stripSplit};
  const int len[2] = {stripSplit, NUM_LEDS - stripSplit};
  const int skip[2] = {settings.ledSkip1, settings.ledSkip2};
  const int trim[2] = {settings.ledTrim1, settings.ledTrim2};
  for (int o = 0; o < 2; o++) {
    if (o == 1 && !strip2) break;
    for (int s = 0; s < len[o]; s++) {
      RgbColor c(0, 0, 0);
      if (s >= simSkip[o] && s < len[o] - simTrim[o]) {
        const int li = base[o] + skip[o] + (s - simSkip[o]);
        if (li < base[o] + len[o] - trim[o]) {
          const uint8_t* px = rgb + li * 3;
          c = RgbColor((uint8_t)(px[colorPerm[0]] * k),
                       (uint8_t)(px[colorPerm[1]] * k),
                       (uint8_t)(px[colorPerm[2]] * k));
        }
      }
      if (o == 0) strip1->SetPixelColor(s, c);
      else strip2->SetPixelColor(s, c);
    }
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

// ---- Home Assistant bridge state ----
HaMqtt haMqtt;
// Last text pushed via MQTT/`/api/text` (song titles). Re-applied whenever
// text-3d becomes active, since a pattern switch reloads NVS params over it.
// A preset that includes its own text still wins (params apply after init).
String g_haText;
String g_currentPreset;  // last loaded preset name; "" after a manual change

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
  const PatternSpecs* mps = specsFor(kPatterns[idx]->id);
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
    float mn = v.substring(c1 + 1, c2).toFloat();
    float mx = v.substring(c2 + 1, c3).toFloat();
    const float rate = v.substring(c3 + 1, c4).toFloat();
    const float step = v.substring(c4 + 1).toFloat();
    // Don't trust the NVS blob: clamp bounds to the spec range (out-of-range
    // mod bounds can push patterns outside their safe input domain — and a
    // bad blob would otherwise re-arm the hazard on every boot).
    const ParamSpec* msp = nullptr;
    if (mps)
      for (int i = 0; i < mps->count; i++)
        if (key == mps->specs[i].key) { msp = &mps->specs[i]; break; }
    if (!msp || msp->type != 0) continue;
    mn = constrain(mn, msp->minV, msp->maxV);
    mx = constrain(mx, msp->minV, msp->maxV);
    mods.set(key.c_str(), mode, mn, mx, rate, step, params.num(key.c_str(), mn),
             msp->stepV, msp->minV);
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

// `persist=false` skips the NVS write — used by transient switches (playlist
// advances, the snake-cal wizard) so automatic cycling doesn't wear flash or
// change what the cube boots into.
void setPatternByIndex(int i, bool persist = true) {
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
  if (!strcmp(activePattern->id, "text-3d") && g_haText.length())
    params.setStr("text", g_haText.c_str());
  g_currentPreset = "";
  haMqtt.markDirty();
  if (persist) saveSetting("pattern", settings.patternId);
  Serial.printf("[pattern] %s\n", activePattern->id);
}

void setPatternById(const String& id, bool persist = true) {
  for (int i = 0; i < kPatternCount; i++) {
    if (id == kPatterns[i]->id) {
      setPatternByIndex(i, persist);
      return;
    }
  }
}

// ---- snake direction calibration ----------------------------------------------
//
// The horizontal D-pad buttons (up/down/left/right) are player-relative: which
// way each points depends on where the player stands. This maps each of those
// four buttons to a cube-relative horizontal SnakeDir (±x / ±y). Vertical
// (z up/down) is invariant to the player's facing, so it bypasses this map.
//
// The wizard (/api/snakecal/*) lights one horizontal cube edge/face at a time
// with an arrow (the "snake-cal" pattern) and asks "which button points here?".
// The button the player taps becomes that cube direction.
//
// snakeDirMap is indexed by button (0=up 1=down 2=left 3=right) and holds a
// SnakeDir value (0..3). Default is the pre-calibration D-pad wiring:
//   up->YP(2)  down->YN(3)  left->XN(1)  right->XP(0)
uint8_t snakeDirMap[4] = {2, 3, 1, 0};

// Prompt order over the four horizontal cube directions.
const SnakeDir kCalTargets[4] = {SnakeDir::XP, SnakeDir::XN, SnakeDir::YP, SnakeDir::YN};
int snakeCalIndex = -1;        // -1 = not calibrating; else 0..3
String snakeCalReturnPattern;  // pattern to restore when the wizard ends
uint32_t snakeCalStartMs = 0;  // for the abandonment timeout
bool snakeCalGuest = false;    // guest wizard: mapping applies in RAM only

int snakeButtonIndex(const String& b) {
  if (b == "up") return 0;
  if (b == "down") return 1;
  if (b == "left") return 2;
  if (b == "right") return 3;
  return -1;
}

const char* snakeDirName(int d) {
  static const char* kNames[6] = {"x+", "x-", "y+", "y-", "z+", "z-"};
  return (d >= 0 && d < 6) ? kNames[d] : "?";
}

void loadSnakeDirMap() {
  prefs.begin("cube", true);
  uint8_t tmp[4];
  const size_t n = prefs.getBytes("snkdir", tmp, sizeof(tmp));
  prefs.end();
  if (n == 4) {
    bool valid = true;
    for (int i = 0; i < 4; i++)
      if (tmp[i] > 3) valid = false;
    if (valid) memcpy(snakeDirMap, tmp, 4);
  }
}

void saveSnakeDirMap() {
  prefs.begin("cube", false);
  prefs.putBytes("snkdir", snakeDirMap, 4);
  prefs.end();
}

// ---- presets ------------------------------------------------------------------
//
// Snapshot the live pattern + its effective params into a named preset file,
// and load one back. Param types mirror effectiveParam/applyParamFromString:
// type 1 = bool, 2/3/4 = enum/palette/string, everything else (0 num, 5 color)
// = number.

// includeText=false (text-3d "params only" save): the "text" param is left
// out of the snapshot, so loading the preset styles the text without
// replacing the words currently showing (song titles from HA, etc.).
void savePresetSnapshot(const String& name, int priority, float dwellSec,
                        bool includeText = true) {
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
      if (!includeText && strcmp(sp.key, "text") == 0) continue;
      switch (sp.type) {
        case 1: p[sp.key] = params.boolean(sp.key, sp.defNum != 0); break;
        case 2:
        case 3:
        case 4: p[sp.key] = params.str(sp.key, sp.defStr); break;
        default: p[sp.key] = params.num(sp.key, sp.defNum); break;
      }
    }
  }
  // Classify music-reactive vs ambient from the snapshot; the owner can flip
  // it later via /api/presets/meta.
  doc["reactive"] = presetLooksReactive(settings.patternId, p);
  // Carry per-param modulation with the preset — ALWAYS emit the object, even
  // when empty, so "no modulation" round-trips (loading a mod-free preset
  // must clear any NVS-loaded mods rather than inherit them).
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
  presetWrite(name, doc);
}

bool loadPresetByName(const String& name, bool persistPattern = true) {
  JsonDocument doc;
  if (!presetRead(name, doc)) return false;
  const char* pat = doc["pattern"] | "";
  // Unknown/typo'd pattern id: fail loudly instead of silently perturbing
  // whatever pattern happens to be active.
  if (!pat[0] || !specsFor(pat)) return false;
  // The words currently showing, captured before the switch clears params —
  // restored below when the preset was saved without its text.
  String liveText;
  if (settings.patternId == "text-3d") liveText = params.str("text", "");
  setPatternById(String(pat), persistPattern);  // clears params, loads NVS defaults, inits
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
        case 4: {
          // as<const char*>() is null for non-string JSON values (hand-edited
          // import); setStr guards too, but skip explicitly here.
          const char* sv = p[sp.key].as<const char*>();
          if (sv) params.setStr(sp.key, sv);
          break;
        }
        default: params.setNum(sp.key, p[sp.key].as<float>()); break;
      }
    }
  }
  // The preset's "mods" object (present — possibly empty — in every preset
  // saved since it was introduced) replaces whatever setPatternById loaded
  // from NVS; an empty object means "this preset runs unmodulated". Legacy
  // presets without the key keep the NVS mods. Numeric params only; bounds
  // clamped to the spec range (out-of-range mod bounds could drive patterns
  // out of their safe input domain).
  if (doc["mods"].is<JsonObject>()) {
    JsonObject m = doc["mods"].as<JsonObject>();
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
      const float mn = constrain(o["min"] | mp->minV, mp->minV, mp->maxV);
      const float mx = constrain(o["max"] | mp->maxV, mp->minV, mp->maxV);
      mods.set(kv.key().c_str(), mode, mn, mx, o["rate"] | 1.0f,
               o["step"] | mp->stepV, params.num(kv.key().c_str(), mp->defNum),
               mp->stepV, mp->minV);
    }
  }
  // Preset saved without its words ("params only"): keep the text that was
  // already up — the live console text first, else the HA text applied by
  // setPatternById, else the pattern's saved default.
  if (settings.patternId == "text-3d" && p["text"].isNull() && liveText.length())
    params.setStr("text", liveText.c_str());
  if (activePattern && activePattern->init) activePattern->init(ctx);
  g_currentPreset = name;
  haMqtt.markDirty();
  return true;
}

// ---- power switch (Home Assistant light on/off) -------------------------------
//
// "Off" blanks the frame and stops rendering; on boards with the energy-saving
// relay it also cuts LED V+ entirely. Network, mic, and the console stay up.
void setCubePower(bool on) {
  if (settings.powerOn == on) return;
  settings.powerOn = on;
  prefs.begin("cube", false);
  prefs.putBool("power", on);
  prefs.end();
  if (!on) {
    memset(frame, 0, sizeof(frame));
    show(frame);
#if CUBE_RELAY_PIN >= 0
    digitalWrite(CUBE_RELAY_PIN, LOW);
#endif
  } else {
#if CUBE_RELAY_PIN >= 0
    digitalWrite(CUBE_RELAY_PIN, HIGH);
#endif
  }
  haMqtt.markDirty();
  Serial.printf("[power] %s\n", on ? "on" : "off");
}

// Song-title text from HA/REST: sanitize, remember, apply live if text-3d is
// up. Shared by the MQTT text entity and POST /api/text.
void setCubeText(const String& v) {
  String t = v.length() > 200 ? v.substring(0, 200) : v;
  for (size_t i = 0; i < t.length();)  // control chars have no glyphs
    if ((uint8_t)t[i] < 0x20) t.remove(i, 1); else i++;
  g_haText = t;
  if (settings.patternId == "text-3d") params.setStr("text", t.c_str());
  haMqtt.markDirty();
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
  String list;            // active curated playlist; "" = all presets (default)
  float dwellOverride = 0;  // >0 = demo mode: every preset shows this long
};
PlaylistState playlist;

// The cycle's on/off switch, shuffle and curated-list choice survive a reset:
// the cube is a fixture, and a power blip shouldn't leave it stuck on one
// pattern. Only *explicit* toggles (console, HA, function button) persist —
// pausePlaylistForManual() stays RAM-only, so a manual preset pick is a
// temporary detour and the next boot returns to the cycle. Enabled defaults
// to ON for a factory cube.
void playlistLoadPrefs() {
  prefs.begin("cube", true);
  playlist.enabled = prefs.getBool("plon", true);
  playlist.shuffle = prefs.getBool("plshuf", false);
  playlist.list = prefs.getString("pllist", "");
  prefs.end();
  // Zero dwell so the first playlistTick() swaps the boot pattern for the
  // playlist's pick as soon as services are up, not 20s later.
  if (playlist.enabled) playlist.dwellSec = 0;
}

// Curated playlists (cube-eq5 follow-on): named subsets of the preset store,
// persisted as one JSON file {"lists":[{"name":..,"presets":[names...]}]}.
// The default cycle (all presets, priority rules) needs no entry here.
constexpr int kMaxPlaylists = 8;
const char* kPlaylistsPath = "/playlists.json";

bool playlistsLoad(JsonDocument& doc) {
  File f = LittleFS.open(kPlaylistsPath, "r");
  if (!f) return false;
  const bool ok = !deserializeJson(doc, f);
  f.close();
  return ok;
}

bool playlistsStore(const JsonDocument& doc) {
  File f = LittleFS.open(kPlaylistsPath, "w");
  if (!f) return false;
  const bool ok = serializeJson(doc, f) > 0;
  f.close();
  return ok;
}

// Keep curated lists consistent when a preset is renamed (to != "") or
// deleted (to == "") — otherwise lists silently thin out over time.
void playlistsFixupPreset(const String& from, const String& to) {
  JsonDocument doc;
  if (!playlistsLoad(doc)) return;
  bool changed = false;
  for (JsonObject l : doc["lists"].as<JsonArray>()) {
    JsonArray ps = l["presets"].as<JsonArray>();
    for (size_t i = 0; i < ps.size();) {
      if (from == (ps[i] | "")) {
        changed = true;
        if (to.length()) {
          ps[i] = to;
          i++;
        } else {
          ps.remove(i);
        }
      } else {
        i++;
      }
    }
  }
  if (changed) playlistsStore(doc);
}
// Set while the playlist engine itself loads a preset, so the auto-pause hook
// (which fires on manual pattern/param/preset changes) doesn't stop the cycle.
bool playlistLoading = false;

// Manual pattern/param/preset change stops the cycle so tinkering isn't stomped.
void pausePlaylistForManual() {
  if (!playlistLoading && playlist.enabled) {
    playlist.enabled = false;
    haMqtt.markDirty();  // HA's playlist switch flips off immediately
  }
}

void playlistLoadIndex(const PresetMeta* metas, int i) {
  playlistLoading = true;
  // persistPattern=false: an automatic advance shouldn't write NVS every
  // dwell (flash wear) or decide what the cube boots into.
  loadPresetByName(metas[i].name, false);
  playlistLoading = false;
  playlist.current = metas[i].name;
  // Demo override beats per-preset dwell (e.g. 5s cycling for filming).
  playlist.dwellSec = playlist.dwellOverride > 0
                          ? playlist.dwellOverride
                          : (metas[i].dwellSec > 0 ? metas[i].dwellSec : 20.0f);
  playlist.startedMs = millis();
}

// Fill `out` with the cycle's preset pool. Default ("" list): the whole
// store. Named list: its members in curated order (missing presets skipped);
// an unknown/empty list falls back to the whole store rather than stalling
// the cycle. Sets `curated` accordingly.
int playlistPool(PresetMeta* out, bool& curated) {
  PresetMeta metas[kMaxPresets];
  const int n = presetList(metas, kMaxPresets);
  curated = false;
  if (playlist.list.length() == 0) {
    for (int i = 0; i < n; i++) out[i] = metas[i];
    return n;
  }
  JsonDocument doc;
  int k = 0;
  if (playlistsLoad(doc)) {
    for (JsonObject l : doc["lists"].as<JsonArray>()) {
      if (playlist.list != (l["name"] | "")) continue;
      for (const char* nm : l["presets"].as<JsonArray>()) {
        if (!nm || k >= kMaxPresets) break;
        for (int i = 0; i < n; i++) {
          if (metas[i].name == nm) {
            out[k++] = metas[i];
            break;
          }
        }
      }
      break;
    }
  }
  if (k == 0) {  // unknown or fully-orphaned list: behave like the default
    for (int i = 0; i < n; i++) out[i] = metas[i];
    return n;
  }
  curated = true;
  return k;
}

// Shuffle pick: weighted by priority, priority-0 excluded, avoiding an
// immediate repeat when another eligible preset exists. With micFilter set,
// music-reactive presets are excluded too (mic off = they'd sit static).
// In a curated list membership is explicit, so nothing is excluded there:
// priority 0 plays with weight 1. Returns -1 if nothing is eligible.
int playlistPickWeighted(const PresetMeta* metas, int n, const String& avoid,
                         bool micFilter, bool curated) {
  auto weight = [&](int i) {
    return curated ? max(1, metas[i].priority) : metas[i].priority;
  };
  auto eligible = [&](int i) {
    return weight(i) > 0 && !(micFilter && metas[i].reactive);
  };
  int elExAvoid = 0;
  for (int i = 0; i < n; i++)
    if (eligible(i) && metas[i].name != avoid) elExAvoid++;
  const bool skipAvoid = elExAvoid > 0;  // only avoid repeat when alternatives exist
  int total = 0;
  for (int i = 0; i < n; i++) {
    if (!eligible(i)) continue;
    if (skipAvoid && metas[i].name == avoid) continue;
    total += weight(i);
  }
  if (total <= 0) return -1;
  int r = (int)(esp_random() % (uint32_t)total);
  for (int i = 0; i < n; i++) {
    if (!eligible(i)) continue;
    if (skipAvoid && metas[i].name == avoid) continue;
    r -= weight(i);
    if (r < 0) return i;
  }
  return -1;
}

// Advance to the next preset. dir = +1 next / -1 prev (in-order only; shuffle
// always re-picks at random regardless of dir sign). Disables the playlist if
// the store is empty or nothing is eligible for shuffle.
void playlistAdvance(int dir) {
  static PresetMeta metas[kMaxPresets];
  bool curated = false;
  const int n = playlistPool(metas, curated);
  if (n == 0) {
    playlist.enabled = false;
    return;
  }
  // Mic off: auto-play only ambient presets (reactive ones would sit static).
  // If that filter empties the pool, ignore it rather than kill the cycle.
  const bool micFilter = !settings.micEnabled || !audioCaptureAvailable();
  int pick;
  if (playlist.shuffle) {
    pick = playlistPickWeighted(metas, n, playlist.current, micFilter, curated);
    if (pick < 0 && micFilter)
      pick = playlistPickWeighted(metas, n, playlist.current, false, curated);
    if (pick < 0) {  // all priority 0 -> nothing to auto-play
      playlist.enabled = false;
      return;
    }
  } else {
    int cur = -1;
    for (int i = 0; i < n; i++)
      if (metas[i].name == playlist.current) { cur = i; break; }
    pick = cur < 0 ? 0 : (((cur + dir) % n) + n) % n;
    if (micFilter) {  // walk past reactive presets; give up after a full lap
      for (int step = 0; step < n && metas[pick].reactive; step++)
        pick = ((pick + dir) % n + n) % n;
    }
  }
  playlistLoadIndex(metas, pick);
}

// Called every loop: when a preset's dwell elapses, roll to the next.
void playlistTick() {
  if (!playlist.enabled) return;
  if ((millis() - playlist.startedMs) / 1000.0f < playlist.dwellSec) return;
  playlistAdvance(+1);
}

// Start/stop the cycle. Starting loads the first preset immediately rather
// than waiting out a dwell. Shared by the owner console and the guest tier.
void playlistSetEnabled(bool en) {
  if (en && !playlist.enabled) {
    playlist.enabled = true;
    playlist.current = "";
    playlistAdvance(+1);  // load the first preset now
  } else {
    playlist.enabled = en;
  }
}

// ---- Home Assistant MQTT bridge ----------------------------------------------

void startHaMqtt() {
  if (!settings.mqttEnabled || settings.mqttHost.length() == 0) return;
  HaMqttHooks hooks;
  hooks.getPower = []() { return settings.powerOn; };
  hooks.setPower = [](bool on) { setCubePower(on); };
  hooks.getBrightness = []() { return settings.brightness; };
  hooks.setBrightness = [](float v) {
    settings.brightness = constrain(v, 0.0f, 1.0f);
    saveSetting("bright", settings.brightness);
  };
  hooks.getPattern = []() { return settings.patternId; };
  hooks.setPattern = [](const String& id) {
    pausePlaylistForManual();
    setPatternById(id);  // unknown ids are ignored
  };
  hooks.getPreset = []() { return g_currentPreset; };
  hooks.loadPreset = [](const String& name) {
    if (loadPresetByName(name)) pausePlaylistForManual();
  };
  hooks.getText = []() {
    return settings.patternId == "text-3d" ? String(params.str("text", "HELLO 123 "))
                                           : g_haText;
  };
  hooks.setText = [](const String& v) { setCubeText(v); };
  hooks.patternOptions = [](JsonArray arr) {
    for (int i = 0; i < kPatternCount; i++) arr.add(kPatterns[i]->id);
  };
  hooks.presetOptions = [](JsonArray arr) {
    PresetMeta metas[kMaxPresets];
    const int n = presetList(metas, kMaxPresets);
    for (int i = 0; i < n; i++) arr.add(metas[i].name);
  };
  hooks.getPlaylistOn = []() { return playlist.enabled; };
  hooks.setPlaylistOn = [](bool on) {
    saveSetting("plon", on);  // HA's switch is an explicit choice — boots stick
    if (on && !playlist.enabled) {
      playlist.enabled = true;
      playlist.current = "";
      playlistAdvance(+1);
    } else if (!on) {
      playlist.enabled = false;
    }
  };
  hooks.getShuffle = []() { return playlist.shuffle; };
  hooks.setShuffle = [](bool on) {
    playlist.shuffle = on;
    saveSetting("plshuf", on);
  };
  hooks.playlistStep = [](int dir) {
    if (!playlist.enabled) {
      playlist.enabled = true;
      saveSetting("plon", true);
    }
    playlistAdvance(dir);
  };
  hooks.getPlaylistList = []() { return playlist.list; };
  hooks.setPlaylistList = [](const String& l) {
    playlist.list = l;
    saveSetting("pllist", l);
    if (playlist.enabled) playlistAdvance(+1);
  };
  hooks.playlistOptions = [](JsonArray arr) {
    JsonDocument doc;
    if (!playlistsLoad(doc)) return;
    for (JsonObject l : doc["lists"].as<JsonArray>()) {
      const char* nm = l["name"] | "";
      // String() forces a copy into the destination doc — `doc` dies when
      // this lambda returns, long before the discovery payload serializes.
      if (nm[0]) arr.add(String(nm));
    }
  };
  hooks.version = CUBE_VERSION;
  haMqtt.begin(settings.mqttHost, settings.mqttPort, settings.mqttUser,
               settings.mqttPass, hooks);
  Serial.printf("[mqtt] broker %s:%u\n", settings.mqttHost.c_str(), settings.mqttPort);
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
    if (!state) {
      // Press = next playlist preset; if the cube is "off", wake it first.
      // The button re-enables a paused cycle rather than stepping raw
      // patterns — it's the no-network way to say "show me something else".
      if (!settings.powerOn) {
        setCubePower(true);
      } else {
        if (!playlist.enabled) saveSetting("plon", true);
        playlist.enabled = true;
        playlistAdvance(+1);
        // Empty preset store: playlistAdvance() bails and disables itself,
        // so fall back to cycling the built-in patterns.
        if (!playlist.enabled) setPatternByIndex(activePatternIdx + 1);
      }
    }
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
// A STA retry is a bounded *attempt*, not a standing state. Left in AP_STA,
// the STA half auto-reconnects forever, rescanning all channels; every scan
// drags the single radio off the AP's channel, so hotspot clients see a
// sluggish console and dropped realtime packets whenever the home SSID is
// out of range (playa mode). We park back on AP-only between attempts.
bool staRetryInFlight = false;

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
        staRetryInFlight = true;  // bounded attempt; parks on AP if it fails
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
          staRetryInFlight = false;
        } else if (staRetryInFlight && now - lastStaRetryMs > 25000) {
          // Attempt failed — quiesce the STA so it stops scanning.
          staRetryInFlight = false;
          WiFi.disconnect();
          WiFi.mode(WIFI_AP);
          Serial.println("[net] sta retry failed; radio parked on AP");
        } else if (!staRetryInFlight && now - lastStaRetryMs > 120000 &&
                   WiFi.softAPgetStationNum() == 0) {
          // Retry only while nobody is on the hotspot: an off-channel scan
          // mid-party stutters everyone. The moment the last client leaves,
          // this condition is already ripe and the retry fires.
          lastStaRetryMs = now;
          staRetryInFlight = true;
          Serial.println("[net] retrying sta join (AP stays up)");
          WiFi.mode(WIFI_AP_STA);
          WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
        }
      }
      break;
  }
}

// ---- OTA arming (reflash prank-proofing) --------------------------------------
//
// ArduinoOTA no longer runs all the time: on a playa/party network anyone
// with espota.py and the password (a guessable default, or shoulder-surfed)
// could push their own firmware. Wireless reflashing is DISARMED until the
// owner opens a time-limited window from the admin page; it re-locks itself
// after 15 minutes, and any reboot disarms. Arming also refuses to run while
// the AP/OTA password is still the factory default. USB flashing can't be
// blocked in software (the ROM bootloader always wins) — that's what a
// locked enclosure is for.
bool otaArmed = false;
uint32_t otaDisarmAtMs = 0;
constexpr uint32_t kOtaWindowMs = 15UL * 60UL * 1000UL;

void armOta() {
  if (!otaArmed) {
    ArduinoOTA.setHostname("cube");
    ArduinoOTA.setPassword(settings.apPass.c_str());
    ArduinoOTA.begin();
    otaArmed = true;
  }
  otaDisarmAtMs = millis() + kOtaWindowMs;  // re-arm extends the window
  Serial.println("[ota] armed (15 min window)");
}

void disarmOta() {
  if (otaArmed) {
    ArduinoOTA.end();
    otaArmed = false;
    Serial.println("[ota] disarmed");
  }
}

// ---- web UI -------------------------------------------------------------------

WebServer server(80);

// One protection space for every owner-only page, so the browser caches a
// single credential across /admin, /leds, /calibrate and /wifi. Realm text
// rides in an HTTP header — keep it ASCII. The fail body is what shows if the
// visitor cancels the prompt, so it says who to log in as.
static const char kAuthRealm[] = "cube owner (username: cube)";
static const char kAuthFailHtml[] =
    "<!doctype html><meta name=viewport content=\"width=device-width\">"
    "<body style=\"font:16px system-ui;padding:2em;max-width:30em\">"
    "<h2>Owner login</h2><p>Log in as <b>cube</b> with the console password. "
    "If no console password has been set, use the cube's Wi-Fi password.</p>"
    "<p><a href=\"/\">Back to the patterns</a></p>";

// Optional console password (HTTP Basic auth, username "cube"). Gates the
// ADMIN tier only: hardware, network, calibration, and anything persistent.
// The guest tier (pattern picking, live params, brightness, game pad) stays
// open so the cube can be handed to a crowd.
bool authed() {
  if (settings.uiPass.length() == 0) return true;
  if (server.authenticate("cube", settings.uiPass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, kAuthRealm, kAuthFailHtml);
  return false;
}

// The secret that proves ownership to a client on the cube's own AP: the
// console password, or — when none has been set — the AP/OTA password, which
// is never empty. That fallback is the whole point: with no console password
// there is otherwise *no* credential an owner can offer, so a cube on its
// fallback AP (home Wi-Fi out of range) locks its owner out of the admin
// pages until it is reflashed over USB. It is a speed bump rather than a
// secret, since everyone on the AP typed that password to join — set a
// console password from /wifi before handing the cube to a crowd.
const String &ownerPass() {
  return settings.uiPass.length() ? settings.uiPass : settings.apPass;
}

// A "guest" is anyone connected to the cube's own SoftAP (typically the
// 192.168.4.x subnet) — a party-goer with a phone. Clients reaching us over
// STA / the home LAN are the owner. We decide by comparing the requester's
// IP against the SoftAP network, so it needs no password and can't be
// spoofed by the UI. When no AP is up (clean STA), there are no guests.
bool isGuestRequest() {
  if (WiFi.getMode() == WIFI_STA) return false;
  // Unconfigured cube (no home Wi-Fi saved): the AP *is* the setup console —
  // whoever is there is the owner. Without this, first boot would 403 the
  // /wifi page and the cube could never be configured.
  if (settings.wifiSsid.length() == 0) return false;
  const IPAddress ap = WiFi.softAPIP();
  const IPAddress cl = server.client().remoteIP();
  if (!(cl[0] == ap[0] && cl[1] == ap[1] && cl[2] == ap[2])) return false;
  // Guest tier disabled (WiFi page): knowing the WPA2 hotspot password IS
  // ownership — everyone who can associate gets the full console.
  if (!settings.apGuests) return false;
  // AP-subnet client — a guest until it proves ownership with ownerPass()
  // (the owner's escape hatch when the cube is on its fallback AP away from
  // home).
  if (server.authenticate("cube", ownerPass().c_str())) return false;
  return true;
}

// Reject owner-only *actions* from guests with a friendly 403. Returns true
// if the request was blocked (caller should return immediately). For API
// routes only: a 401 here would pop a login box in the middle of the guest
// app, and the page that issued the fetch already had its chance to log in.
bool guestBlocked() {
  if (isGuestRequest()) {
    server.send(403, "text/plain",
                "That's owner-only. Guests can play with patterns and knobs — "
                "and hit the reset button — but can't change saved settings.");
    return true;
  }
  return false;
}

// Owner-only *pages*: ask for the password instead of refusing. Whoever lands
// on /admin gets the browser's login prompt — a guest cancels out of it, the
// owner types the password and is in. Never answer a page with a bare 403:
// with no prompt attached that is a dead end with no way forward, which is
// exactly what this replaces.
bool guestChallenged() {
  if (!isGuestRequest()) return false;
  server.requestAuthentication(BASIC_AUTH, kAuthRealm, kAuthFailHtml);
  return true;
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
  json += ",\"micAvail\":" + String(audioCaptureAvailable() ? "true" : "false");
  json += ",\"power\":" + String(settings.powerOn ? "true" : "false");
  json += ",\"mqtt\":\"" + String(settings.mqttEnabled ? haMqtt.status() : "off") + "\"";
  json += ",\"otaArmed\":" + String(otaArmed ? "true" : "false");
  json += ",\"otaRemainingSec\":" +
          String(otaArmed ? max(0, (int)((int32_t)(otaDisarmAtMs - millis()) / 1000)) : 0);
  json += ",\"guest\":" + String(isGuestRequest() ? "true" : "false");
  json += ",\"live\":" +
          String((int32_t)(liveUntilMs - millis()) > 0 ? "true" : "false");
  json += ",\"up\":\"" + settings.upAxis + "\"";
  json += ",\"ledPin\":" + String(settings.ledPin) + ",\"ledPin2\":" + String(settings.ledPin2) +
          ",\"ledSplit\":" + String(settings.ledSplit);
  json += ",\"ledSkip1\":" + String(settings.ledSkip1) + ",\"ledTrim1\":" + String(settings.ledTrim1) +
          ",\"ledSkip2\":" + String(settings.ledSkip2) + ",\"ledTrim2\":" + String(settings.ledTrim2);
  json += ",\"simSkip1\":" + String(simSkip[0]) + ",\"simTrim1\":" + String(simTrim[0]) +
          ",\"simSkip2\":" + String(simSkip[1]) + ",\"simTrim2\":" + String(simTrim[1]);
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
  String plList = playlist.list;
  plList.replace("\\", "\\\\");
  plList.replace("\"", "\\\"");
  json += ",\"playlist\":{\"enabled\":" + String(playlist.enabled ? "true" : "false") +
          ",\"shuffle\":" + String(playlist.shuffle ? "true" : "false") +
          ",\"current\":\"" + plCur + "\",\"dwellRemainingSec\":" + String(dwellRem, 0) +
          ",\"list\":\"" + plList +
          "\",\"dwellOverrideSec\":" + String(playlist.dwellOverride, 0) + "}";
  json += ",\"version\":\"" CUBE_VERSION "\"}";
  server.send(200, "application/json", json);
}

void setupWebServer();
bool servicesStarted = false;

// mDNS, the realtime UDP listener, and the HTTP console. Must only run
// once a network interface exists; starting them during the STA join
// corrupts the WiFi blob's management-frame callbacks (InstructionFetchError
// in sta_recv_mgmt) and crash-loops the chip. OTA is deliberately absent —
// see armOta() above.
void startNetServices() {
  if (servicesStarted) return;
  servicesStarted = true;
  MDNS.begin(CUBE_MDNS_NAME);  // http://<name>.local/
  udp.begin(kRealtimePort);
  setupWebServer();
  audioCaptureStart();
  audioCaptureReconfigure(settings.micLeft, settings.micSquelch);
  startHaMqtt();
  Serial.println("[net] services up (mdns/udp/http/mic; ota disarmed)");
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
    haMqtt.markDirty();
    server.send(200, "text/plain", "ok");
  });
  // Guest-tier power switch (mirrors the HA light entity).
  server.on("/api/power", HTTP_POST, []() {
    setCubePower(server.arg("on") != "0");
    server.send(200, "text/plain", "ok");
  });
  // Set the text-3d message (REST twin of the MQTT text entity — handy for
  // song-title automations without a broker). Optional show=1 switches the
  // cube to text-3d if it isn't there already.
  server.on("/api/text", HTTP_POST, []() {
    setCubeText(server.arg("v"));
    if (server.arg("show") == "1" && settings.patternId != "text-3d") {
      pausePlaylistForManual();
      setPatternById("text-3d", /*persist=*/false);
    }
    server.send(200, "text/plain", "ok");
  });
  // Album-art upload for the image-3d pattern. Body = base64 of raw RGB24
  // (w*h*3 bytes, row-major from the top row); w/h query params up to 64.
  // The image is box-downscaled to 10x10 on arrival. persist=0 skips the
  // flash write (recommended for once-per-song automations); show=1 switches
  // the cube to image-3d.
  server.on("/api/image", HTTP_POST, []() {
    const int w = server.hasArg("w") ? server.arg("w").toInt() : IMG_N;
    const int h = server.hasArg("h") ? server.arg("h").toInt() : IMG_N;
    if (w < 1 || h < 1 || w > IMG_MAX_SRC || h > IMG_MAX_SRC) {
      server.send(400, "text/plain", "w/h must be 1..64");
      return;
    }
    String b64 = server.arg("plain");
    // Tolerate the newlines/whitespace that `base64` pipelines emit.
    String clean;
    clean.reserve(b64.length());
    for (size_t i = 0; i < b64.length(); i++)
      if ((uint8_t)b64[i] > ' ') clean += b64[i];
    // Heap, not static: 12KB of BSS doesn't fit in DRAM alongside the WiFi
    // stack; a short-lived alloc during one request is fine.
    const size_t rawCap = IMG_MAX_SRC * IMG_MAX_SRC * 3;
    uint8_t* raw = (uint8_t*)malloc(rawCap);
    if (!raw) {
      server.send(500, "text/plain", "out of memory");
      return;
    }
    size_t olen = 0;
    if (mbedtls_base64_decode(raw, rawCap, &olen, (const uint8_t*)clean.c_str(),
                              clean.length()) != 0 ||
        (int)olen < w * h * 3) {
      free(raw);
      server.send(400, "text/plain", "body must be base64 of w*h*3 RGB bytes");
      return;
    }
    cubeImageSet(raw, w, h);
    free(raw);
    if (server.arg("persist") != "0") {
      File f = LittleFS.open("/image.rgb", "w");
      if (f) {
        f.write(cubeImagePixels(), IMG_N * IMG_N * 3);
        f.close();
      }
    }
    if (server.arg("show") == "1" && settings.patternId != "image-3d") {
      pausePlaylistForManual();
      setPatternById("image-3d", /*persist=*/false);
    }
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/supply", HTTP_POST, []() {
    if (guestBlocked() || !authed()) return;
    settings.supplyMA = (uint32_t)server.arg("ma").toInt();
    saveSetting("supply", settings.supplyMA);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/order", HTTP_POST, []() {
    if (guestBlocked() || !authed()) return;
    settings.colorOrder = server.arg("v");
    applyColorOrder(settings.colorOrder);
    saveSetting("order", settings.colorOrder);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/scan", HTTP_GET, []() {
    if (guestBlocked() || !authed()) return;
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
    if (guestBlocked() || !authed()) return;
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
    if (guestBlocked() || !authed()) return;
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
      // Escape the value: string params (e.g. text-3d's text) are user input
      // and a stray quote/backslash/control char would break the whole JSON.
      String val = isActive ? effectiveParam(sp)
                            : (sp.type == 0 ? String(sp.defNum, 3)
                               : sp.type == 1 ? String(sp.defNum != 0 ? "1" : "0")
                                              : String(sp.defStr));
      val.replace("\\", "\\\\");
      val.replace("\"", "\\\"");
      for (size_t vi = 0; vi < val.length(); vi++)
        if ((uint8_t)val[vi] < 0x20) val.setCharAt(vi, ' ');
      // Hover help; authored text may contain quotes, so escape like value.
      String dsc = sp.desc ? sp.desc : "";
      dsc.replace("\\", "\\\\");
      dsc.replace("\"", "\\\"");
      json += "{\"key\":\"" + String(sp.key) + "\",\"label\":\"" + String(sp.label) +
              "\",\"type\":" + String(sp.type) + ",\"min\":" + String(sp.minV, 3) +
              ",\"max\":" + String(sp.maxV, 3) + ",\"step\":" + String(sp.stepV, 3) +
              ",\"options\":\"" + String(sp.options) + "\",\"desc\":\"" + dsc +
              "\",\"value\":\"" + val + "\"";
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
  // Arm/disarm the wireless-reflash window (see armOta above). Owner-only,
  // and arming refuses to run on the factory-default password.
  server.on("/api/ota", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (server.arg("on") == "1") {
      if (settings.apPass == CUBE_AP_PASS) {
        server.send(400, "text/plain",
                    "AP/OTA password is still the factory default — set your own "
                    "on the WiFi page before arming updates");
        return;
      }
      armOta();
    } else {
      disarmOta();
    }
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
              ",\"dwellSec\":" + String(metas[i].dwellSec, 0) +
              ",\"reactive\":" + String(metas[i].reactive ? "true" : "false") + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });
  // Snapshot the live pattern + params as a named preset. Owner-only.
  server.on("/api/presets/save", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    String name = server.arg("name");
    // Strip control chars (they'd corrupt the hand-built JSON list output).
    for (size_t i = 0; i < name.length();)
      if ((uint8_t)name[i] < 0x20) name.remove(i, 1); else i++;
    if (name.length() == 0) {
      server.send(400, "text/plain", "name required");
      return;
    }
    // Enforce the cap here like import does — an over-cap write would succeed
    // on disk but be invisible to the list/playlist/export.
    {
      PresetMeta metas[kMaxPresets];
      const int n = presetList(metas, kMaxPresets);
      bool exists = false;
      for (int i = 0; i < n; i++)
        if (presetSlug(metas[i].name) == presetSlug(name)) { exists = true; break; }
      if (!exists && n >= kMaxPresets) {
        server.send(507, "text/plain", "preset limit reached (40) — delete some first");
        return;
      }
    }
    const int priority =
        server.hasArg("priority") ? constrain((int)server.arg("priority").toInt(), 0, 5) : 3;
    const float dwell = server.hasArg("dwellSec")
                            ? max(1.0f, server.arg("dwellSec").toFloat())
                            : 20.0f;
    // saveText=0: leave the words out of a text-3d preset (style-only).
    const bool saveText = server.arg("saveText") != "0";
    savePresetSnapshot(name, priority, dwell, saveText);
    haMqtt.refreshDiscovery();  // preset select options changed
    server.send(200, "text/plain", "ok");
  });
  // Apply a preset (changes the live pattern) — allowed for guests.
  server.on("/api/presets/load", HTTP_POST, []() {
    if (!loadPresetByName(server.arg("name"))) {
      server.send(404, "text/plain", "no such preset");
      return;  // a failed load shouldn't kill a running cycle
    }
    pausePlaylistForManual();  // loading a single preset stops the cycle
    server.send(200, "text/plain", "ok");
  });
  // Delete a preset. Owner-only.
  server.on("/api/presets/delete", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    const String name = server.arg("name");
    if (name.length() == 0) {
      server.send(400, "text/plain", "name required");
      return;
    }
    presetDelete(name);
    if (playlist.current == name) playlist.current = "";  // cursor restarts cleanly
    playlistsFixupPreset(name, "");
    haMqtt.refreshDiscovery();
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
    if (server.hasArg("reactive")) doc["reactive"] = server.arg("reactive") == "1";
    // Clamp dwell >= 1s: 0/garbage would make the live playlist advance every
    // frame (a strobe of preset switches).
    const float dwell = max(1.0f, server.arg("dwellSec").toFloat());
    if (server.hasArg("dwellSec")) doc["dwellSec"] = dwell;
    if (!presetWrite(name, doc)) {
      server.send(500, "text/plain", "write failed");
      return;
    }
    // Reflect a new dwell on the live preset immediately.
    if (playlist.enabled && playlist.current == name && server.hasArg("dwellSec"))
      playlist.dwellSec = dwell;
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
    // Whole-body-in-RAM parse: cap the size so a huge upload can't OOM the
    // chip (40 legit presets ≈ 25KB; 64KB is generous headroom).
    if (server.arg("plain").length() > 64 * 1024) {
      server.send(413, "text/plain", "too large (max 64KB)");
      return;
    }
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
      // Clamp hand-edited metadata to the same ranges save/meta enforce.
      out["priority"] = constrain((int)(out["priority"] | 3), 0, 5);
      out["dwellSec"] = max(1.0f, (float)(out["dwellSec"] | 20.0f));
      if (out["reactive"].isNull())  // hand-authored file: classify it
        out["reactive"] =
            presetLooksReactive(String(pat), out["params"].as<JsonObjectConst>());
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
    haMqtt.refreshDiscovery();
    server.send(200, "application/json",
                "{\"imported\":" + String(imported) + ",\"overwritten\":" +
                    String(overwritten) + ",\"skipped\":" + String(skipped) + "}");
  });
  // Rename a preset's display name. Writes the preset under the new slug and
  // deletes the old file only after that write succeeds. Renaming onto an
  // EXISTING different preset is rejected (409) — silent overwrite through a
  // rename prompt is a data-loss trap; delete the target first if intended.
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
    const bool slugChanged = presetSlug(from) != presetSlug(to);
    if (slugChanged) {
      JsonDocument tmp;
      if (presetRead(to, tmp)) {
        server.send(409, "text/plain", "a preset with that name already exists");
        return;
      }
    }
    doc["name"] = to;
    if (!presetWrite(to, doc)) {  // never delete the original on a failed write
      server.send(500, "text/plain", "write failed");
      return;
    }
    if (slugChanged) presetDelete(from);
    if (playlist.current == from) playlist.current = to;
    playlistsFixupPreset(from, to);
    haMqtt.refreshDiscovery();
    server.send(200, "text/plain", "ok");
  });
  // ---- playlist cycling ----
  // Configure/toggle the cycle. Owner-only, EXCEPT a guest may play/pause so a
  // party-goer can stop the rotation on a pattern they like and start it again
  // afterwards. Everything else (shuffle, list, dwell) stays owner-only.
  server.on("/api/playlist", HTTP_POST, []() {
    if (isGuestRequest()) {
      if (server.hasArg("enabled")) {
        // Deliberately no saveSetting(): a guest's toggle lives in RAM only,
        // so a reset restores the owner's console choice.
        playlistSetEnabled(server.arg("enabled") == "1");
        haMqtt.markDirty();
        server.send(200, "text/plain", "ok");
      } else {
        guestBlocked();  // sends the 403
      }
      return;
    }
    if (!authed()) return;
    if (server.hasArg("shuffle")) {
      playlist.shuffle = server.arg("shuffle") == "1";
      saveSetting("plshuf", playlist.shuffle);
    }
    // Curated list selection ("" = all presets). Switching mid-cycle re-picks
    // so the change is visible immediately.
    if (server.hasArg("list")) {
      playlist.list = server.arg("list");
      saveSetting("pllist", playlist.list);
      if (playlist.enabled) playlistAdvance(+1);
    }
    // Demo dwell override (0 = per-preset times). Applies to the preset
    // showing right now too — "cycle at 5s" should not wait out a 60s dwell.
    if (server.hasArg("dwell")) {
      playlist.dwellOverride =
          constrain(server.arg("dwell").toFloat(), 0.0f, 3600.0f);
      if (playlist.enabled && playlist.dwellOverride > 0)
        playlist.dwellSec = playlist.dwellOverride;
    }
    if (server.hasArg("enabled")) {
      const bool en = server.arg("enabled") == "1";
      saveSetting("plon", en);  // owner's console toggle survives a reset
      playlistSetEnabled(en);
    }
    haMqtt.markDirty();  // transport state changed — sync HA promptly
    server.send(200, "text/plain", "ok");
  });
  // ---- curated playlists ----
  // List all saved playlists + the active selection. Open to guests (the
  // sidebar shows it); mutation below is owner-only.
  server.on("/api/playlists", HTTP_GET, []() {
    JsonDocument doc;
    playlistsLoad(doc);
    JsonDocument out;
    out["active"] = playlist.list;
    out["dwellOverrideSec"] = playlist.dwellOverride;
    JsonArray lists = out["lists"].to<JsonArray>();
    for (JsonObject l : doc["lists"].as<JsonArray>()) lists.add(l);
    String json;
    serializeJson(out, json);
    server.send(200, "application/json", json);
  });
  // Create/replace one playlist: ?name=<n>, body = JSON array of preset
  // names in play order. Owner-only.
  server.on("/api/playlists/save", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    String name = server.arg("name");
    for (size_t i = 0; i < name.length();)
      if ((uint8_t)name[i] < 0x20) name.remove(i, 1); else i++;
    if (name.length() == 0) {
      server.send(400, "text/plain", "name required");
      return;
    }
    JsonDocument body;
    if (deserializeJson(body, server.arg("plain")) || !body.is<JsonArray>()) {
      server.send(400, "text/plain", "body must be a JSON array of preset names");
      return;
    }
    JsonDocument doc;
    playlistsLoad(doc);
    JsonArray lists = doc["lists"].isNull() ? doc["lists"].to<JsonArray>()
                                            : doc["lists"].as<JsonArray>();
    JsonObject mine;
    for (JsonObject l : lists)
      if (name == (l["name"] | "")) { mine = l; break; }
    if (mine.isNull()) {
      if ((int)lists.size() >= kMaxPlaylists) {
        server.send(507, "text/plain", "playlist limit reached (8) — delete one first");
        return;
      }
      mine = lists.add<JsonObject>();
      mine["name"] = name;
    }
    JsonArray ps = mine["presets"].to<JsonArray>();  // to<> clears any old members
    int count = 0;
    for (const char* nm : body.as<JsonArray>()) {
      if (nm && count < kMaxPresets) {
        ps.add(nm);
        count++;
      }
    }
    if (!playlistsStore(doc)) {
      server.send(500, "text/plain", "write failed");
      return;
    }
    haMqtt.refreshDiscovery();  // the HA collection picker's options changed
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/playlists/delete", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    const String name = server.arg("name");
    JsonDocument doc;
    playlistsLoad(doc);
    JsonArray lists = doc["lists"].as<JsonArray>();
    bool found = false;
    for (size_t i = 0; i < lists.size(); i++) {
      if (name == (lists[i]["name"] | "")) {
        lists.remove(i);
        found = true;
        break;
      }
    }
    if (!found) {
      server.send(404, "text/plain", "no such playlist");
      return;
    }
    playlistsStore(doc);
    if (playlist.list == name) playlist.list = "";  // fall back to the default pool
    haMqtt.refreshDiscovery();
    server.send(200, "text/plain", "ok");
  });
  // Manual skip (owner-only). Enables the cycle if it was off.
  server.on("/api/playlist/next", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (!playlist.enabled) { playlist.enabled = true; }
    playlistAdvance(+1);
    haMqtt.markDirty();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/playlist/prev", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    if (!playlist.enabled) { playlist.enabled = true; }
    playlistAdvance(-1);
    haMqtt.markDirty();
    server.send(200, "text/plain", "ok");
  });
  // Runtime LED hardware config: pins + single/dual split + snipped-LED
  // compensation. Applies live (strips are rebuilt) and persists.
  server.on("/api/ledcfg", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    settings.ledPin = server.arg("pin").toInt();
    settings.ledPin2 = server.hasArg("pin2") ? server.arg("pin2").toInt() : -1;
    settings.ledSplit = server.hasArg("split") ? server.arg("split").toInt() : NUM_LEDS;
    if (server.hasArg("skip1"))
      settings.ledSkip1 = constrain(server.arg("skip1").toInt(), 0, NUM_LEDS - 1);
    if (server.hasArg("trim1"))
      settings.ledTrim1 = constrain(server.arg("trim1").toInt(), 0, NUM_LEDS - 1);
    if (server.hasArg("skip2"))
      settings.ledSkip2 = constrain(server.arg("skip2").toInt(), 0, NUM_LEDS - 1);
    if (server.hasArg("trim2"))
      settings.ledTrim2 = constrain(server.arg("trim2").toInt(), 0, NUM_LEDS - 1);
    saveHardware();
    initStrips();
    server.send(200, "text/plain", "ok");
  });
  // Debug: emulate a snipped chain on intact hardware (see show()). Runtime
  // only — deliberately never persisted, so a reboot clears the simulation.
  // Missing args read as 0, i.e. posting with no args clears everything.
  server.on("/api/ledsim", HTTP_POST, []() {
    if (guestBlocked()) return;
    if (!authed()) return;
    simSkip[0] = constrain(server.arg("sim1").toInt(), 0, NUM_LEDS);
    simTrim[0] = constrain(server.arg("simtrim1").toInt(), 0, NUM_LEDS);
    simSkip[1] = constrain(server.arg("sim2").toInt(), 0, NUM_LEDS);
    simTrim[1] = constrain(server.arg("simtrim2").toInt(), 0, NUM_LEDS);
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
    json += "],\"bpm\":" + String(af.bpm, 1) + ",\"frames\":" + String(st.frames) +
            ",\"rms\":" + String(st.lastRms, 1) +
            ",\"dc\":" + String(st.lastDc, 1) + ",\"rawMin\":" + String(st.rawMin) +
            ",\"rawMax\":" + String(st.rawMax) +
            ",\"mic\":\"" + String(audioCaptureKind()) + "\"" +
            ",\"avail\":" + String(audioCaptureAvailable() ? "true" : "false") +
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
    if (guestBlocked() || !authed()) return;
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
    // Console-typed words are as sticky as HA-pushed ones: they survive
    // pattern swaps and style-only preset loads.
    if (key == "text" && settings.patternId == "text-3d") g_haText = v;
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
    // Clamp to the spec range: a mod driving a param outside it can crash
    // patterns (e.g. fire baseLayers indexes an array by the value).
    mn = constrain(mn, sp->minV, sp->maxV);
    mx = constrain(mx, sp->minV, sp->maxV);
    mods.set(key.c_str(), mode, mn, mx, rate, step,
             params.num(key.c_str(), sp->defNum), sp->stepV, sp->minV);
    server.send(200, "text/plain", "ok");
  });
  // Body: text lines "led,x,y,z". Returns candidates/suggestion as JSON.
  server.on("/api/calibrate/solve", HTTP_POST, []() {
    if (guestBlocked() || !authed()) return;
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
  // Admin HTML pages answer an unproven visitor with a challenge, never a
  // 403: authed() prompts when a console password is set, guestChallenged()
  // prompts when the request comes from the AP subnet, and valid credentials
  // make isGuestRequest() false. Between them there is always a prompt to
  // answer, which is how the owner gets in from the fallback AP.
  // API routes keep guestBlocked() first: the browser reuses cached page
  // credentials there, and guests should get the friendly 403, not a popup.
  server.on("/calibrate", HTTP_GET, []() {
    if (!authed()) return;
    if (guestChallenged()) return;
    server.send(200, "text/html", kCalibrateHtml);
  });
  server.on("/admin", HTTP_GET, []() {
    if (!authed()) return;
    if (guestChallenged()) return;
    server.send(200, "text/html", kAdminHtml);
  });
  server.on("/leds", HTTP_GET, []() {
    if (!authed()) return;
    if (guestChallenged()) return;
    server.send(200, "text/html", kLedsHtml);
  });
  // Game pad: intentionally NOT auth-gated so guests can play snake/pacman
  // without the console password. Input queueing is harmless.
  server.on("/snake", HTTP_GET, []() { server.send(200, "text/html", kSnakeHtml); });
  server.on("/api/game", HTTP_POST, []() {
    // Two input forms:
    //   btn=up|down|left|right — player-relative horizontal, run through the
    //     calibrated direction map to a cube-relative ±x/±y SnakeDir.
    //   dir=0..5              — direct SnakeDir (used for the invariant z
    //     up/down buttons and any programmatic control). Kept for compat.
    int d;
    if (server.hasArg("btn")) {
      const int bi = snakeButtonIndex(server.arg("btn"));
      if (bi < 0) {
        server.send(400, "text/plain", "bad btn");
        return;
      }
      d = snakeDirMap[bi];
    } else {
      d = server.arg("dir").toInt();
      if (d < 0 || d > 5) {
        server.send(400, "text/plain", "bad dir");
        return;
      }
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
  // ---- snake direction calibration wizard ----
  // Start: remember the current pattern, switch to the arrow visual, prompt 0.
  // Deliberately guest-open (players calibrate for where they stand), but a
  // guest's mapping is applied in RAM only — never saved to NVS.
  server.on("/api/snakecal/start", HTTP_POST, []() {
    pausePlaylistForManual();  // a dwell rollover mid-wizard would swap the visual
    if (settings.patternId != "snake-cal")  // double-start keeps the ORIGINAL return
      snakeCalReturnPattern = settings.patternId;
    snakeCalIndex = 0;
    snakeCalStartMs = millis();
    snakeCalGuest = isGuestRequest();
    setPatternById("snake-cal", /*persist=*/false);
    params.setNum("dir", (float)(int)kCalTargets[0]);
    server.send(200, "text/plain", "ok");
  });
  // State: current prompt for the page ("which button points at this edge?").
  server.on("/api/snakecal/state", HTTP_GET, []() {
    String json = "{\"active\":";
    json += (snakeCalIndex >= 0) ? "true" : "false";
    json += ",\"index\":" + String(snakeCalIndex < 0 ? 0 : snakeCalIndex);
    json += ",\"total\":4,\"target\":\"";
    json += (snakeCalIndex >= 0) ? snakeDirName((int)kCalTargets[snakeCalIndex]) : "";
    json += "\",\"map\":{\"up\":\"" + String(snakeDirName(snakeDirMap[0])) + "\",\"down\":\"" +
            String(snakeDirName(snakeDirMap[1])) + "\",\"left\":\"" +
            String(snakeDirName(snakeDirMap[2])) + "\",\"right\":\"" +
            String(snakeDirName(snakeDirMap[3])) + "\"}}";
    server.send(200, "application/json", json);
  });
  // Map: the tapped button becomes the currently-prompted cube direction.
  server.on("/api/snakecal/map", HTTP_POST, []() {
    if (snakeCalIndex < 0) {
      server.send(409, "text/plain", "not calibrating");
      return;
    }
    const int bi = snakeButtonIndex(server.arg("button"));
    if (bi < 0) {
      server.send(400, "text/plain", "bad button");
      return;
    }
    snakeDirMap[bi] = (uint8_t)(int)kCalTargets[snakeCalIndex];
    snakeCalIndex++;
    if (snakeCalIndex >= 4) {
      if (!snakeCalGuest) saveSnakeDirMap();  // guests: live mapping only
      snakeCalIndex = -1;
      setPatternById(snakeCalReturnPattern.length() ? snakeCalReturnPattern : String("snake-3d"),
                     /*persist=*/false);
      server.send(200, "application/json", "{\"done\":true}");
    } else {
      params.setNum("dir", (float)(int)kCalTargets[snakeCalIndex]);
      server.send(200, "application/json", "{\"done\":false}");
    }
  });
  // Cancel: abandon the wizard, restore the prior pattern (map unchanged).
  server.on("/api/snakecal/cancel", HTTP_POST, []() {
    snakeCalIndex = -1;
    setPatternById(snakeCalReturnPattern.length() ? snakeCalReturnPattern : String("snake-3d"),
                   /*persist=*/false);
    server.send(200, "text/plain", "ok");
  });
  server.on("/wifi", HTTP_GET, []() {
    if (!authed()) return;
    if (guestChallenged()) return;
    String page = kWifiHtml;
    page.replace("%SSID%", settings.wifiSsid);
    page.replace("%UIPASS%", settings.uiPass.length() ? "(unchanged)" : "(not set)");
    page.replace("%APGUESTS%", settings.apGuests ? "checked" : "");
    page.replace("%MQEN%", settings.mqttEnabled ? "checked" : "");
    page.replace("%MQHOST%", settings.mqttHost);
    page.replace("%MQPORT%", String(settings.mqttPort));
    page.replace("%MQUSER%", settings.mqttUser);
    page.replace("%APWARN%",
                 settings.apPass == CUBE_AP_PASS
                     ? "<p style=\"background:#3a1d1d;border:1px solid #8a4438;"
                       "color:#f0b0a0;border-radius:6px;padding:10px\">⚠ Still the "
                       "factory password (<b>cubelight</b>) — anyone who's seen the "
                       "project can join the hotspot. Set your own before taking "
                       "the cube out in public. Wireless reflashing won't arm "
                       "until you do.</p>"
                     : "");
    server.send(200, "text/html", page);
  });
  server.on("/wifi", HTTP_POST, []() {
    if (!authed()) return;
    if (guestChallenged()) return;
    if (server.hasArg("ssid")) saveSetting("ssid", server.arg("ssid"));
    if (server.arg("pass").length() > 0) saveSetting("pass", server.arg("pass"));
    if (server.arg("appass").length() >= 8) saveSetting("appass", server.arg("appass"));
    if (server.hasArg("clearui")) saveSetting("uipass", String(""));
    else if (server.arg("uipass").length() >= 4) saveSetting("uipass", server.arg("uipass"));
    prefs.begin("cube", false);
    prefs.putBool("apguests", server.hasArg("apguests"));
    prefs.putBool("mqen", server.hasArg("mqen"));
    if (server.hasArg("mqhost")) prefs.putString("mqhost", server.arg("mqhost"));
    if (server.arg("mqport").toInt() > 0)
      prefs.putUInt("mqport", (uint32_t)server.arg("mqport").toInt());
    if (server.hasArg("mquser")) prefs.putString("mquser", server.arg("mquser"));
    if (server.arg("mqpass").length() > 0) prefs.putString("mqpass", server.arg("mqpass"));
    prefs.end();
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
  loadSnakeDirMap();  // player-relative D-pad -> cube-direction map
  presetsBegin();  // mount LittleFS for preset storage
  playlistLoadPrefs();  // restore the auto-cycle (default: on)
  applyColorOrder(settings.colorOrder);
  applyGeometry();
#if CUBE_BUTTON_PIN >= 0
  pinMode(CUBE_BUTTON_PIN, INPUT_PULLUP);
  // Physical-access rescue: hold the function button through power-on for 3
  // full seconds to clear a lost console password. That password gates both
  // /admin and OTA arming, so a mistyped (or browser-autofilled) value would
  // otherwise mean opening the case for the serial pads. Active-low with a
  // pullup: a missing or unpressed button reads HIGH and can never trigger.
  if (digitalRead(CUBE_BUTTON_PIN) == LOW) {
    uint32_t heldMs = 0;
    while (digitalRead(CUBE_BUTTON_PIN) == LOW && heldMs < 3000) {
      delay(50);
      heldMs += 50;
    }
    if (heldMs >= 3000) {
      saveSetting("uipass", String(""));
      settings.uiPass = "";
      Serial.println("[rescue] console password cleared (button held at boot)");
    }
  }
#endif
#if CUBE_RELAY_PIN >= 0
  pinMode(CUBE_RELAY_PIN, OUTPUT);
  // Power the LED string — unless the cube was switched "off" (HA light)
  // before the reboot, in which case stay dark until switched on.
  digitalWrite(CUBE_RELAY_PIN, settings.powerOn ? HIGH : LOW);
#endif
  initStrips();
  // Restore the persisted album art (uploaded via /api/image).
  {
    File f = LittleFS.open("/image.rgb", "r");
    if (f) {
      uint8_t buf[IMG_N * IMG_N * 3];
      if (f.read(buf, sizeof(buf)) == sizeof(buf)) cubeImageSet(buf, IMG_N, IMG_N);
      f.close();
    }
  }

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
  if (otaArmed) {
    ArduinoOTA.handle();
    if ((int32_t)(millis() - otaDisarmAtMs) > 0) disarmOta();  // window lapsed
  }
  server.handleClient();
  if (WiFi.status() == WL_CONNECTED) haMqtt.loop();
  handleRealtime();
  handleButton();
  playlistTick();
  // Snake-cal wizard watchdog: restore the prior pattern if the page was
  // abandoned (5 min), and abort the wizard if something else (the physical
  // button) switched the pattern out from under it.
  if (snakeCalIndex >= 0) {
    if (millis() - snakeCalStartMs > 5UL * 60UL * 1000UL) {
      snakeCalIndex = -1;
      setPatternById(snakeCalReturnPattern.length() ? snakeCalReturnPattern
                                                    : String("snake-3d"),
                     /*persist=*/false);
    } else if (settings.patternId != "snake-cal") {
      snakeCalIndex = -1;
    }
  }

  static uint32_t nextFrameMs = 0;
  const uint32_t now = millis();
  if (now < nextFrameMs) return;
  nextFrameMs = now + 1000 / CUBE_FPS;

  // "Off" (HA light switch): the frame was blanked (and the relay dropped)
  // by setCubePower; skip rendering — including live DNRGB — until on again.
  if (!settings.powerOn) return;

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
