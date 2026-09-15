// WebAssembly bridge: exposes the firmware pattern engine (lib/core, compiled
// verbatim) to the browser simulator through a small C API. No pattern code
// lives here — the browser runs the exact pat_*.cpp files the ESP32 runs.
//
// Build: simulator/build.sh (emcc). Every exported function is plain C so
// the JS side can call it through Module.ccall / cwrap without embind.

#include <emscripten/emscripten.h>

#include <cstdio>
#include <cstring>
#include <string>

#include "cube_audio.h"
#include "cube_image.h"
#include "cube_pacman.h"
#include "cube_palettes.h"
#include "cube_param_specs.h"
#include "cube_pattern.h"
#include "cube_snake.h"

using namespace cube;

namespace {

uint8_t g_buffer[NUM_LEDS * 3];
Geometry g_geo;
AudioFrame g_audio;
Params g_params;
PatternCtx g_ctx{g_buffer, &g_geo, 0, 0, &g_audio, &g_params};
const Pattern* g_pattern = nullptr;
BeatDetector g_beat;
std::string g_json;  // scratch for JSON exports (pointer stays valid until next call)

void jsonEscape(std::string& out, const char* s) {
  for (; *s; s++) {
    switch (*s) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      default: out += *s;
    }
  }
}

}  // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE int cube_num_leds() { return NUM_LEDS; }
EMSCRIPTEN_KEEPALIVE int cube_n() { return CUBE_N; }

EMSCRIPTEN_KEEPALIVE int cube_pattern_count() { return kPatternCount; }
EMSCRIPTEN_KEEPALIVE const char* cube_pattern_id(int i) {
  return (i >= 0 && i < kPatternCount) ? kPatterns[i]->id : "";
}

// Select a pattern by id; resets the clock and runs its init. Params are
// kept (the console does the same) — call cube_clear_params() first for
// factory defaults. Returns 0 on unknown id.
EMSCRIPTEN_KEEPALIVE int cube_select(const char* id) {
  const Pattern* p = findPattern(id);
  if (!p) return 0;
  g_pattern = p;
  std::memset(g_buffer, 0, sizeof(g_buffer));
  g_ctx.t = 0;
  g_ctx.dt = 0;
  if (p->init) p->init(g_ctx);
  return 1;
}

EMSCRIPTEN_KEEPALIVE void cube_clear_params() { g_params.clear(); }
EMSCRIPTEN_KEEPALIVE void cube_set_num(const char* k, float v) { g_params.setNum(k, v); }
EMSCRIPTEN_KEEPALIVE void cube_set_bool(const char* k, int v) { g_params.setBool(k, v != 0); }
EMSCRIPTEN_KEEPALIVE void cube_set_str(const char* k, const char* v) { g_params.setStr(k, v); }

// Design orientation, same six choices as the console ("z+","x-", ...).
EMSCRIPTEN_KEEPALIVE void cube_set_up(const char* up) {
  g_geo.rebuild(Layout{}, upAxisFromString(up));
}

// Audio in. Bands/level are 0..1 already normalized (the JS side mirrors
// audio_capture.cpp's per-band floor/peak math); beat/bpm come from the
// shared BeatDetector below unless the caller overrides them.
EMSCRIPTEN_KEEPALIVE void cube_set_audio(float level, const float* bands, float beat, float bpm) {
  g_audio.level = level;
  for (int i = 0; i < AUDIO_BANDS; i++) g_audio.bands[i] = bands[i];
  g_audio.beat = beat;
  g_audio.bpm = bpm;
}

// BeatDetector (cube_audio.cpp) — feed band-0 energy per audio frame, decay
// per render frame, read envelope/bpm. Same code path as the firmware.
EMSCRIPTEN_KEEPALIVE int cube_beat_feed(float bass, unsigned nowMs) { return g_beat.onFrame(bass, nowMs); }
EMSCRIPTEN_KEEPALIVE void cube_beat_decay(float dt) { g_beat.decay(dt); }
EMSCRIPTEN_KEEPALIVE float cube_beat_envelope() { return g_beat.envelope(); }
EMSCRIPTEN_KEEPALIVE float cube_beat_bpm(unsigned nowMs) { return g_beat.bpm(nowMs); }
EMSCRIPTEN_KEEPALIVE void cube_beat_reset() { g_beat.reset(); }

// Render one frame at pattern-time t (seconds since select) and write the
// RGB buffer. Returns the buffer pointer (NUM_LEDS*3 bytes, wire order for
// the default layout, i.e. index = geo.idx(x,y,z)).
EMSCRIPTEN_KEEPALIVE uint8_t* cube_render(float t) {
  if (!g_pattern) return g_buffer;
  g_ctx.dt = t - g_ctx.t;
  if (g_ctx.dt < 0) g_ctx.dt = 0;
  g_ctx.t = t;
  g_pattern->render(g_ctx);
  return g_buffer;
}
EMSCRIPTEN_KEEPALIVE uint8_t* cube_buffer() { return g_buffer; }

// (x,y,z) -> LED index for the current geometry, so the renderer can place
// each buffer entry in space.
EMSCRIPTEN_KEEPALIVE int cube_index(int x, int y, int z) { return g_geo.idx(x, y, z); }

// Games: the phone game pad's six directions.
EMSCRIPTEN_KEEPALIVE void cube_snake_input(int dir) { queueSnakeInput((SnakeDir)dir); }
EMSCRIPTEN_KEEPALIVE void cube_pacman_input(int dir) { queuePacmanInput((SnakeDir)dir); }

// image-3d: push an RGB24 image (<= 64x64); the core box-averages to 10x10.
EMSCRIPTEN_KEEPALIVE int cube_image_set(const uint8_t* rgb, int w, int h) {
  return cubeImageSet(rgb, w, h) ? 1 : 0;
}

// Param specs for every pattern as JSON, straight from cube_param_specs.h,
// so the simulator's controls are the console's controls.
EMSCRIPTEN_KEEPALIVE const char* cube_specs_json() {
  g_json = "{";
  for (int i = 0; i < kPatternSpecsCount; i++) {
    const PatternSpecs& ps = kPatternSpecs[i];
    if (i) g_json += ",";
    g_json += "\"";
    jsonEscape(g_json, ps.id);
    g_json += "\":[";
    char num[64];
    for (int j = 0; j < ps.count; j++) {
      const ParamSpec& s = ps.specs[j];
      if (j) g_json += ",";
      g_json += "{\"key\":\"";
      jsonEscape(g_json, s.key);
      g_json += "\",\"label\":\"";
      jsonEscape(g_json, s.label);
      std::snprintf(num, sizeof(num), "\",\"type\":%d,\"min\":%g,\"max\":%g,\"step\":%g,\"def\":%g",
                    s.type, s.minV, s.maxV, s.stepV, s.defNum);
      g_json += num;
      g_json += ",\"defStr\":\"";
      jsonEscape(g_json, s.defStr);
      g_json += "\",\"options\":\"";
      jsonEscape(g_json, s.options);
      g_json += "\",\"desc\":\"";
      jsonEscape(g_json, s.desc);
      g_json += "\"}";
    }
    g_json += "]";
  }
  g_json += "}";
  return g_json.c_str();
}

EMSCRIPTEN_KEEPALIVE const char* cube_palettes_json() {
  g_json = "[";
  for (int i = 0; i < kPaletteNameCount; i++) {
    if (i) g_json += ",";
    g_json += "\"";
    jsonEscape(g_json, kPaletteNames[i]);
    g_json += "\"";
  }
  g_json += "]";
  return g_json.c_str();
}

}  // extern "C"
