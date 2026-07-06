// ESP32 firmware entry point (GL-C-618WL target).
//
// Status: skeleton, written ahead of hardware arrival — compiles under
// PlatformIO but is UNTESTED on a real board. Verify CUBE_LED_PIN and the
// color order against the stock WLED settings before first flash.
//
// What it does:
//  - Joins WiFi (or brings up AP "cube-light" if no credentials are built in)
//  - Runs the shared pattern core at CUBE_FPS into a frame buffer
//  - Pushes frames to the LEDs via NeoPixelBus (RMT, hardware-timed)
//  - Listens for WLED DNRGB packets on UDP 21324; live packets override the
//    local pattern until their timeout lapses (same semantics as WLED), so
//    the existing Node dev server keeps working unchanged
//  - ArduinoOTA so re-flashing never needs the USB cable after the first time
//
// Not yet here (next steps): mic capture + FFT, WS/HTTP control API,
// LittleFS persistence, brightness/orientation, snake input.

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <NeoPixelBus.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#include "cube_pattern.h"

#ifndef CUBE_LED_PIN
#define CUBE_LED_PIN 16
#endif
#ifndef CUBE_FPS
#define CUBE_FPS 30
#endif

using namespace cube;

// WS2811 12V strings: color order varies by batch — stock WLED's LED settings
// page shows the right one for this cube. NeoRgbFeature/NeoGrbFeature/NeoBrgFeature.
using ColorFeature = NeoRgbFeature;
using Method = NeoEsp32Rmt0Ws2811Method;

NeoPixelBus<ColorFeature, Method> strip(NUM_LEDS, CUBE_LED_PIN);

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
PatternCtx ctx{frame, &geo, 0, 0, &audio, &params};
uint32_t patternStartMs = 0;
float lastT = 0;

void setPattern(const char* id) {
  const Pattern* p = findPattern(id);
  if (!p) p = findPattern(kDefaultPatternId);
  activePattern = p;
  patternStartMs = millis();
  lastT = 0;
  ctx.t = 0;
  ctx.dt = 0;
  if (p->init) p->init(ctx);
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

void show(const uint8_t* rgb) {
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.SetPixelColor(i, RgbColor(rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]));
  }
  strip.Show();
}

void setup() {
  Serial.begin(115200);
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

  setPattern(kDefaultPatternId);
}

void loop() {
  ArduinoOTA.handle();
  handleRealtime();

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
