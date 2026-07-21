#pragma once

// Per-param automatic modulation (cube-la3) — an ADVANCED layer that drifts a
// NUMERIC pattern param over time without touching pattern render code. Two
// modes:
//   PINGPONG — value scrubs smoothly min<->max as a triangle wave. `rate` is
//              the sweep speed in *value units per second* (a half sweep of
//              (max-min) units takes (max-min)/rate seconds). `step` is UNUSED.
//   WALK     — every 1/`rate` seconds the value takes a random +/- step of up
//              to `step` units (clamped to [min,max]). `rate` is steps/sec.
//              At a bound the next step is forced inward, so it never sticks.
//
// Lives in firmware/src because it uses Arduino millis()/esp_random — it must
// NOT go in lib/core, which the native harness compiles without Arduino.

#include <Arduino.h>
#include <esp_random.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "cube_param_specs.h"
#include "cube_params.h"

namespace cube {

class ModStore {
 public:
  enum Mode : uint8_t { OFF = 0, PINGPONG = 1, WALK = 2 };

  struct Entry {
    char key[Params::kKeyLen];
    uint8_t mode;
    float minV, maxV, rate, step;
    // runtime state (not persisted)
    float phase;      // pingpong triangle phase, 0..2 (0..1 up, 1..2 down)
    float value;      // walk current value
    uint32_t lastMs;  // pingpong: last tick; walk: last step time
    bool started;
  };

  void clear() { count_ = 0; }
  int count() const { return count_; }
  const Entry* at(int i) const { return &entries_[i]; }

  Entry* find(const char* key) {
    for (int i = 0; i < count_; i++)
      if (std::strcmp(entries_[i].key, key) == 0) return &entries_[i];
    return nullptr;
  }

  static const char* modeName(uint8_t m) {
    return m == PINGPONG ? "pingpong" : m == WALK ? "walk" : "off";
  }
  static uint8_t modeFromName(const char* s) {
    if (!std::strcmp(s, "pingpong")) return PINGPONG;
    if (!std::strcmp(s, "walk")) return WALK;
    return OFF;
  }

  // Sensible defaults for a spec + mode when the caller omits the numbers.
  static void defaults(const ParamSpec& sp, uint8_t mode, float& mn, float& mx,
                       float& rate, float& step) {
    mn = sp.minV;
    mx = sp.maxV;
    float range = mx - mn;
    if (range <= 0) range = 1;
    step = sp.stepV > 0 ? sp.stepV * 4.0f : range / 20.0f;
    if (step <= 0) step = range / 20.0f;
    // pingpong: ~4s per half sweep; walk: 2 steps/sec.
    rate = (mode == WALK) ? 2.0f : range / 4.0f;
    if (rate <= 0) rate = 1.0f;
  }

  // Register/update a mod for `key`. mode OFF removes it. `curVal` seeds the
  // starting value/phase so enabling doesn't jump.
  void set(const char* key, uint8_t mode, float mn, float mx, float rate,
           float step, float curVal) {
    if (mode == OFF) {
      remove(key);
      return;
    }
    Entry* e = find(key);
    if (!e) {
      if (count_ >= Params::kMax) return;
      e = &entries_[count_++];
      std::strncpy(e->key, key, Params::kKeyLen - 1);
      e->key[Params::kKeyLen - 1] = '\0';
      e->started = false;
    }
    e->mode = mode;
    e->minV = mn;
    e->maxV = mx;
    e->rate = rate;
    e->step = step;
    if (!e->started) {
      float v = curVal;
      if (v < mn) v = mn;
      if (v > mx) v = mx;
      e->value = v;
      float range = mx - mn;
      e->phase = range > 0 ? (v - mn) / range : 0.0f;  // upward leg
      e->lastMs = millis();
      e->started = true;
    }
  }

  void remove(const char* key) {
    for (int i = 0; i < count_; i++)
      if (std::strcmp(entries_[i].key, key) == 0) {
        entries_[i] = entries_[--count_];
        return;
      }
  }

  // Advance every mod and write its live value into `params` so render() sees
  // it. Call once per frame with millis().
  void tick(Params& params, uint32_t now) {
    for (int i = 0; i < count_; i++) {
      Entry& e = entries_[i];
      const float range = e.maxV - e.minV;
      if (e.mode == PINGPONG) {
        if (range <= 0) {
          params.setNum(e.key, e.minV);
          continue;
        }
        float dt = (int32_t)(now - e.lastMs) / 1000.0f;
        e.lastMs = now;
        if (dt < 0) dt = 0;
        if (dt > 0.5f) dt = 0.5f;  // ignore long gaps (e.g. live override)
        e.phase += (e.rate / range) * dt;
        e.phase = fmodf(e.phase, 2.0f);
        if (e.phase < 0) e.phase += 2.0f;
        const float tri = e.phase <= 1.0f ? e.phase : 2.0f - e.phase;
        params.setNum(e.key, e.minV + range * tri);
      } else if (e.mode == WALK) {
        const float period = e.rate > 0 ? 1000.0f / e.rate : 1000.0f;
        int guard = 0;
        while ((int32_t)(now - e.lastMs) >= (int32_t)period && guard++ < 32) {
          e.lastMs += (uint32_t)period;
          const float mag =
              e.step * (0.3f + 0.7f * (esp_random() / 4294967295.0f));
          int dir;
          if (e.value >= e.maxV) dir = -1;
          else if (e.value <= e.minV) dir = 1;
          else dir = (esp_random() & 1) ? 1 : -1;
          e.value += dir * mag;
          if (e.value < e.minV) e.value = e.minV;
          if (e.value > e.maxV) e.value = e.maxV;
        }
        // Re-sync the clock if we blew past the guard (long stall).
        if ((int32_t)(now - e.lastMs) >= (int32_t)period) e.lastMs = now;
        params.setNum(e.key, e.value);
      }
    }
  }

 private:
  Entry entries_[Params::kMax];
  int count_ = 0;
};

}  // namespace cube
