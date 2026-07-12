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

#include "cube_pattern.h"
#include "cube_power.h"
#include "web_ui.h"

#define CUBE_VERSION "0.2.0"

#ifndef CUBE_LED_PIN
#define CUBE_LED_PIN 16
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
#define CUBE_BUTTON_PIN 0  // Gledopto function button is usually BOOT/GPIO0; -1 disables
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
  String patternId;
  String colorOrder;  // "RGB", "GRB", ...
  float brightness;   // 0..1
  uint32_t supplyMA;  // 0 = limiter off
};

Preferences prefs;
Settings settings;

void loadSettings() {
  prefs.begin("cube", true);
  settings.wifiSsid = prefs.getString("ssid", "");
  settings.wifiPass = prefs.getString("pass", "");
  settings.apPass = prefs.getString("appass", CUBE_AP_PASS);
  settings.patternId = prefs.getString("pattern", kDefaultPatternId);
  settings.colorOrder = prefs.getString("order", CUBE_COLOR_ORDER);
  settings.brightness = prefs.getFloat("bright", 1.0f);
  settings.supplyMA = prefs.getUInt("supply", CUBE_SUPPLY_MA);
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
// applied while copying into the strip.
using Method = NeoEsp32Rmt0Ws2811Method;
NeoPixelBus<NeoRgbFeature, Method> strip(NUM_LEDS, CUBE_LED_PIN);

uint8_t colorPerm[3] = {0, 1, 2};  // perm[wireSlot] = source channel (0=R 1=G 2=B)

void applyColorOrder(const String& order) {
  for (int i = 0; i < 3 && i < (int)order.length(); i++) {
    colorPerm[i] = order[i] == 'G' ? 1 : order[i] == 'B' ? 2 : 0;
  }
}

void show(const uint8_t* rgb) {
  const float limit = currentLimitScale(rgb, NUM_LEDS, CUBE_PER_LED_MA,
                                        CUBE_IDLE_MA_PER_LED, settings.supplyMA);
  const float k = limit * settings.brightness;
  for (int i = 0; i < NUM_LEDS; i++) {
    const uint8_t* px = rgb + i * 3;
    strip.SetPixelColor(i, RgbColor((uint8_t)(px[colorPerm[0]] * k),
                                    (uint8_t)(px[colorPerm[1]] * k),
                                    (uint8_t)(px[colorPerm[2]] * k)));
  }
  strip.Show();
}

// ---- pattern engine -----------------------------------------------------------

Geometry geo;
Params params;
AudioFrame audio;  // zeros until the mic stage lands
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

// ---- web UI -------------------------------------------------------------------

WebServer server(80);

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
  json += ",\"version\":\"" CUBE_VERSION "\"}";
  server.send(200, "application/json", json);
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() { server.send(200, "text/html", kIndexHtml); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/pattern", HTTP_POST, []() {
    setPatternById(server.arg("id"));
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/brightness", HTTP_POST, []() {
    settings.brightness = constrain(server.arg("v").toFloat(), 0.0f, 1.0f);
    saveSetting("bright", settings.brightness);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/supply", HTTP_POST, []() {
    settings.supplyMA = (uint32_t)server.arg("ma").toInt();
    saveSetting("supply", settings.supplyMA);
    server.send(200, "text/plain", "ok");
  });
  server.on("/api/order", HTTP_POST, []() {
    settings.colorOrder = server.arg("v");
    applyColorOrder(settings.colorOrder);
    saveSetting("order", settings.colorOrder);
    server.send(200, "text/plain", "ok");
  });
  server.on("/wifi", HTTP_GET, []() {
    String page = kWifiHtml;
    page.replace("%SSID%", settings.wifiSsid);
    server.send(200, "text/html", page);
  });
  server.on("/wifi", HTTP_POST, []() {
    if (server.hasArg("ssid")) saveSetting("ssid", server.arg("ssid"));
    if (server.arg("pass").length() > 0) saveSetting("pass", server.arg("pass"));
    if (server.arg("appass").length() >= 8) saveSetting("appass", server.arg("appass"));
    server.send(200, "text/html",
                "<body style=\"font-family:system-ui;background:#0d0d10;color:#ddd\">"
                "Saved. Rebooting&hellip;</body>");
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
#if CUBE_BUTTON_PIN >= 0
  pinMode(CUBE_BUTTON_PIN, INPUT_PULLUP);
#endif
  strip.Begin();
  strip.Show();  // all off

  bool joined = false;
  if (settings.wifiSsid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname("cube");
    WiFi.begin(settings.wifiSsid.c_str(), settings.wifiPass.c_str());
    const uint32_t deadline = millis() + 15000;
    while (WiFi.status() != WL_CONNECTED && millis() < deadline) delay(100);
    joined = WiFi.status() == WL_CONNECTED;
  }
  if (!joined) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("cube-light", settings.apPass.c_str());
  }
  Serial.printf("[net] %s ip=%s\n", joined ? "sta" : "ap",
                joined ? WiFi.localIP().toString().c_str()
                       : WiFi.softAPIP().toString().c_str());

  MDNS.begin("cube");  // http://cube.local/
  ArduinoOTA.setHostname("cube");
  ArduinoOTA.setPassword(settings.apPass.c_str());
  ArduinoOTA.begin();
  udp.begin(kRealtimePort);
  setupWebServer();

  setPatternById(settings.patternId);
  if (!activePattern) setPatternByIndex(0);
}

void loop() {
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

  const float t = (now - patternStartMs) / 1000.0f;
  ctx.t = t;
  ctx.dt = t - lastT;
  lastT = t;
  activePattern->render(ctx);
  show(frame);
}
