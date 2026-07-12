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
//  - Joins WiFi (or brings up AP "cube-light" if no credentials built in)
//  - Runs the shared pattern core at CUBE_FPS into a frame buffer
//  - Applies brightness + a WLED-style current limiter (CUBE_SUPPLY_MA /
//    CUBE_PER_LED_MA), then pushes frames via NeoPixelBus (RMT)
//  - Color order is runtime data (CUBE_COLOR_ORDER, e.g. "RGB"/"GRB"/"BRG")
//    applied when copying into the strip, so no rebuild needed to fix it
//  - Listens for WLED DNRGB packets on UDP 21324; live packets override the
//    local pattern until their timeout lapses (same semantics as WLED), so
//    the existing Node dev server keeps working unchanged
//  - Function button (CUBE_BUTTON_PIN, active-low) short-press cycles to the
//    next pattern
//  - ArduinoOTA so re-flashing never needs the USB cable after the first time
//
// Not yet here (next steps): mic capture + FFT -> BeatDetector, WS/HTTP
// control API, LittleFS persistence, /snake controller page.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <NeoPixelBus.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "cube_pattern.h"
#include "cube_power.h"

#ifndef CUBE_LED_PIN
#define CUBE_LED_PIN 16
#endif
#ifndef CUBE_FPS
#define CUBE_FPS 30
#endif
#ifndef CUBE_COLOR_ORDER
#define CUBE_COLOR_ORDER "RGB"  // wire order; check stock WLED's LED settings
#endif
#ifndef CUBE_SUPPLY_MA
#define CUBE_SUPPLY_MA 10000  // PSU budget in mA; 0 disables the limiter
#endif
#ifndef CUBE_PER_LED_MA
#define CUBE_PER_LED_MA 15  // full-white draw of one LED (12V seed pixels ~15mA)
#endif
#ifndef CUBE_IDLE_MA_PER_LED
#define CUBE_IDLE_MA_PER_LED 0.5f  // quiescent IC draw per LED
#endif
#ifndef CUBE_BUTTON_PIN
#define CUBE_BUTTON_PIN 0  // Gledopto function button is usually BOOT/GPIO0; -1 disables
#endif
#ifndef CUBE_BRIGHTNESS
#define CUBE_BRIGHTNESS 1.0f  // global master brightness 0..1
#endif

using namespace cube;

// The NeoPixelBus feature is fixed at RGB; the configured color order is a
// runtime permutation applied while copying the frame into the strip. That
// keeps "it's actually GRB" a one-flag (eventually one-setting) fix.
using Method = NeoEsp32Rmt0Ws2811Method;
NeoPixelBus<NeoRgbFeature, Method> strip(NUM_LEDS, CUBE_LED_PIN);

// perm[wireSlot] = source channel index (0=R 1=G 2=B of the pattern buffer).
uint8_t colorPerm[3] = {0, 1, 2};

void parseColorOrder(const char* order) {
  for (int i = 0; i < 3 && order[i]; i++) {
    colorPerm[i] = order[i] == 'G' ? 1 : order[i] == 'B' ? 2 : 0;
  }
}

WiFiUDP udp;
constexpr uint16_t kRealtimePort = 21324;
constexpr int kUdpBufSize = 4 + 489 * 3;
uint8_t udpBuf[kUdpBufSize];

uint8_t frame[NUM_LEDS * 3];      // pattern render target
uint8_t liveFrame[NUM_LEDS * 3];  // DNRGB override buffer
uint32_t liveUntilMs = 0;

Geometry geo;
Params params;
AudioFrame audio;  // zeros until the mic stage lands
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
  Serial.printf("[pattern] %s\n", activePattern->id);
}

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

// Short-press on the function button cycles patterns. Active-low with the
// internal pullup; 50ms debounce.
void handleButton() {
#if CUBE_BUTTON_PIN >= 0
  static uint32_t lastEdgeMs = 0;
  static bool lastState = true;
  const bool state = digitalRead(CUBE_BUTTON_PIN);
  const uint32_t now = millis();
  if (state != lastState && now - lastEdgeMs > 50) {
    lastEdgeMs = now;
    lastState = state;
    if (!state) setPatternByIndex(activePatternIdx + 1);  // falling edge = press
  }
#endif
}

void show(const uint8_t* rgb) {
  // Brightness + current limit apply at output time only — the pattern
  // buffer stays untouched so previews/telemetry would see full values.
  const float limit = currentLimitScale(rgb, NUM_LEDS, CUBE_PER_LED_MA,
                                        CUBE_IDLE_MA_PER_LED, CUBE_SUPPLY_MA);
  const float k = limit * CUBE_BRIGHTNESS;
  for (int i = 0; i < NUM_LEDS; i++) {
    const uint8_t* px = rgb + i * 3;
    strip.SetPixelColor(i, RgbColor((uint8_t)(px[colorPerm[0]] * k),
                                    (uint8_t)(px[colorPerm[1]] * k),
                                    (uint8_t)(px[colorPerm[2]] * k)));
  }
  strip.Show();
}

void setup() {
  Serial.begin(115200);
  parseColorOrder(CUBE_COLOR_ORDER);
#if CUBE_BUTTON_PIN >= 0
  pinMode(CUBE_BUTTON_PIN, INPUT_PULLUP);
#endif
  strip.Begin();
  strip.Show();  // all off

#if defined(CUBE_WIFI_SSID) && defined(CUBE_WIFI_PASS)
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("cube");
  WiFi.begin(CUBE_WIFI_SSID, CUBE_WIFI_PASS);
  const uint32_t deadline = millis() + 15000;
  while (WiFi.status() != WL_CONNECTED && millis() < deadline) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("cube-light");
  }
#else
  WiFi.mode(WIFI_AP);
  WiFi.softAP("cube-light");
#endif
  Serial.printf("[net] ip=%s\n",
                WiFi.getMode() == WIFI_AP ? WiFi.softAPIP().toString().c_str()
                                          : WiFi.localIP().toString().c_str());

  ArduinoOTA.setHostname("cube");
  ArduinoOTA.begin();
  udp.begin(kRealtimePort);

  setPatternByIndex(0);
}

void loop() {
  ArduinoOTA.handle();
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
