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

#include "audio_capture.h"
#include "cube_calibration.h"
#include "cube_pacman.h"
#include "cube_pattern.h"
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
// applied while copying into the strip. Two buses on separate RMT channels
// transmit concurrently.
#if CUBE_LED_PIN2 >= 0
NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2811Method> strip(CUBE_LED_SPLIT, CUBE_LED_PIN);
NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt1Ws2811Method> strip2(NUM_LEDS - CUBE_LED_SPLIT,
                                                            CUBE_LED_PIN2);
#else
NeoPixelBus<NeoRgbFeature, NeoEsp32Rmt0Ws2811Method> strip(NUM_LEDS, CUBE_LED_PIN);
#endif

uint8_t colorPerm[3] = {0, 1, 2};  // perm[wireSlot] = source channel (0=R 1=G 2=B)

void applyColorOrder(const String& order) {
  for (int i = 0; i < 3 && i < (int)order.length(); i++) {
    colorPerm[i] = order[i] == 'G' ? 1 : order[i] == 'B' ? 2 : 0;
  }
}

void show(const uint8_t* rgb) {
#ifdef CUBE_LED_BISECT_DISABLE
  (void)rgb;
  return;
#endif
  const float limit = currentLimitScale(rgb, NUM_LEDS, CUBE_PER_LED_MA,
                                        CUBE_IDLE_MA_PER_LED, settings.supplyMA);
  const float k = limit * settings.brightness;
  for (int i = 0; i < NUM_LEDS; i++) {
    const uint8_t* px = rgb + i * 3;
    const RgbColor c((uint8_t)(px[colorPerm[0]] * k), (uint8_t)(px[colorPerm[1]] * k),
                     (uint8_t)(px[colorPerm[2]] * k));
#if CUBE_LED_PIN2 >= 0
    if (i < CUBE_LED_SPLIT) strip.SetPixelColor(i, c);
    else strip2.SetPixelColor(i - CUBE_LED_SPLIT, c);
#else
    strip.SetPixelColor(i, c);
#endif
  }
  strip.Show();
#if CUBE_LED_PIN2 >= 0
  strip2.Show();
#endif
}

// ---- pattern engine -----------------------------------------------------------

Geometry geo;

void applyGeometry() {
  geo.rebuild(settings.layout, upAxisFromString(settings.upAxis.c_str()));
}

Params params;
AudioFrame audio;  // refreshed each frame from the mic capture task
uint8_t frame[NUM_LEDS * 3];
const Pattern* activePattern = nullptr;
int activePatternIdx = 0;
PatternCtx ctx{frame, &geo, 0, 0, &audio, &params};
uint32_t patternStartMs = 0;
float lastT = 0;

void setPatternByIndex(int i) {
  activePatternIdx = ((i % kPatternCount) + kPatternCount) % kPatternCount;
  activePattern = kPatterns[activePatternIdx];
  patternStartMs = millis();
  lastT = 0;
  ctx.t = 0;
  ctx.dt = 0;
  params.clear();
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

// Optional console password (HTTP Basic auth, username "cube"). Applied to
// everything the server exposes — view pages included, since the pattern
// controls are on them.
bool authed() {
  if (settings.uiPass.length() == 0) return true;
  if (server.authenticate("cube", settings.uiPass.c_str())) return true;
  server.requestAuthentication();
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
  json += ",\"up\":\"" + settings.upAxis + "\"";
  json += ",\"layout\":{\"flipX\":" + String(settings.layout.flipX ? "true" : "false") +
          ",\"flipY\":" + String(settings.layout.flipY ? "true" : "false") +
          ",\"flipZ\":" + String(settings.layout.flipZ ? "true" : "false") +
          ",\"ledOffset\":" + String(settings.layout.ledOffset) + "}";
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
  Serial.println("[net] services up (mdns/ota/udp/http/mic)");
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    if (!authed()) return;
    server.send(200, "text/html", kIndexHtml);
  });
  server.on("/api/status", HTTP_GET, []() {
    if (!authed()) return;
    handleStatus();
  });
  server.on("/api/pattern", HTTP_POST, []() {
    if (!authed()) return;
    setPatternById(server.arg("id"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/brightness", HTTP_POST, []() {
    if (!authed()) return;
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
  server.on("/api/up", HTTP_POST, []() {
    if (!authed()) return;
    settings.upAxis = server.arg("v");
    applyGeometry();
    saveLayoutAndUp();
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/layout", HTTP_POST, []() {
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
    if (!authed()) return;
    const String key = server.arg("key");
    const String v = server.arg("v");
    const String type = server.arg("type");
    if (type == "str") params.setStr(key.c_str(), v.c_str());
    else if (type == "bool") params.setBool(key.c_str(), v == "true" || v == "1");
    else params.setNum(key.c_str(), v.toFloat());
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
    if (!authed()) return;
    server.send(200, "text/html", kCalibrateHtml);
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
      queueSnakeInput((SnakeDir)d);
      server.send(200, "text/plain", "ok");
    } else if (settings.patternId == "pacman-3d") {
      queuePacmanInput((SnakeDir)d);
      server.send(200, "text/plain", "ok");
    } else {
      server.send(200, "text/plain", "switch the cube to Snake or Pac-Man first");
    }
  });
  server.on("/wifi", HTTP_GET, []() {
    if (!authed()) return;
    String page = kWifiHtml;
    page.replace("%SSID%", settings.wifiSsid);
    page.replace("%UIPASS%", settings.uiPass.length() ? "(unchanged)" : "(not set)");
    server.send(200, "text/html", page);
  });
  server.on("/wifi", HTTP_POST, []() {
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
  applyColorOrder(settings.colorOrder);
  applyGeometry();
#if CUBE_BUTTON_PIN >= 0
  pinMode(CUBE_BUTTON_PIN, INPUT_PULLUP);
#endif
#if CUBE_RELAY_PIN >= 0
  pinMode(CUBE_RELAY_PIN, OUTPUT);
  digitalWrite(CUBE_RELAY_PIN, HIGH);  // power the LED string
#endif
#ifndef CUBE_LED_BISECT_DISABLE
  strip.Begin();
  strip.Show();  // all off
#endif
#if CUBE_LED_PIN2 >= 0 && !defined(CUBE_LED_BISECT_DISABLE)
  strip2.Begin();
  strip2.Show();
#endif

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

  static uint32_t nextFrameMs = 0;
  const uint32_t now = millis();
  if (now < nextFrameMs) return;
  nextFrameMs = now + 1000 / CUBE_FPS;

  if (now < liveUntilMs) {
    show(liveFrame);  // dev server (or any WLED sender) has the cube
    return;
  }

  audioCaptureRead(audio);
  const float t = (now - patternStartMs) / 1000.0f;
  ctx.t = t;
  ctx.dt = t - lastT;
  lastT = t;
  activePattern->render(ctx);
  show(frame);
}
