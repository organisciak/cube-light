#pragma once

// Beat detector — port of the onset-detection math in server/index.ts.
// Mic-agnostic: feed it the bass-band energy (band 0 of the 8 log-spaced
// bands) each audio frame and it maintains the 0..1 beat envelope that
// patterns read from AudioFrame::beat.
//
// Detection model (same constants as the server):
//  - Rolling ~3s history of bass energy acts as a noise floor. The floor
//    average is computed from OLDER samples only (the newest few excluded)
//    so a loud kick can't raise the threshold it has to clear.
//  - A beat fires when bass exceeds floor*1.3, rises 0.04 above the previous
//    sample, clears a minimum absolute level, and the debounce has elapsed.
//  - The envelope snaps to 1 on a beat and decays linearly over 0.25s.

#include <cstdint>

namespace cube {

class BeatDetector {
 public:
  /**
   * Feed one audio frame. `bass` is band-0 energy 0..1; `nowMs` is a
   * monotonic millisecond clock (millis() on-chip). Returns true if this
   * frame is a detected onset.
   */
  bool onFrame(float bass, uint32_t nowMs);

  /** Advance the envelope decay. Call once per render frame. */
  void decay(float dt);

  /** Current beat envelope, 0..1. */
  float envelope() const { return envelope_; }

  /**
   * Tempo estimate in BPM from the median of recent onset intervals, or 0
   * when there's no lock (too few plausible intervals, or the last onset is
   * stale). Patterns treat 0 as "no tempo known".
   */
  float bpm(uint32_t nowMs) const;

  void reset();

 private:
  static constexpr int kHistorySize = 90;       // ~3s at 30Hz
  static constexpr int kFloorExclude = 5;       // newest samples skipped from floor
  static constexpr uint32_t kMinIntervalMs = 180;
  static constexpr float kThresholdMult = 1.3f;
  static constexpr float kMinLevel = 0.10f;
  static constexpr float kMinRise = 0.04f;
  static constexpr float kDecayS = 0.25f;
  // Tempo: intervals outside 250..1500ms (240..40 BPM) are discarded as
  // missed/double-fired onsets; the estimate expires 2.5s after the last one.
  static constexpr int kIntervals = 8;
  static constexpr uint32_t kIntervalMinMs = 250;
  static constexpr uint32_t kIntervalMaxMs = 1500;
  static constexpr uint32_t kBpmStaleMs = 2500;

  float history_[kHistorySize] = {0};
  int historyLen_ = 0;
  int historyHead_ = 0;  // ring buffer write index
  uint32_t lastBeatMs_ = 0;
  float envelope_ = 0;
  uint16_t intervals_[kIntervals] = {0};  // onset-to-onset gaps, ms, ring
  int intervalLen_ = 0;
  int intervalHead_ = 0;
};

}  // namespace cube
