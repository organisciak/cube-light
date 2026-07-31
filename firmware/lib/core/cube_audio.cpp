#include "cube_audio.h"

#include <cmath>

namespace cube {

void BeatDetector::reset() {
  historyLen_ = 0;
  historyHead_ = 0;
  lastBeatMs_ = 0;
  envelope_ = 0;
  intervalLen_ = 0;
  intervalHead_ = 0;
}

bool BeatDetector::onFrame(float bass, uint32_t nowMs) {
  // Previous sample = most recently written history entry.
  const float prevBass =
      historyLen_ > 0
          ? history_[(historyHead_ + kHistorySize - 1) % kHistorySize]
          : 0.0f;

  // Push into the ring buffer.
  history_[historyHead_] = bass;
  historyHead_ = (historyHead_ + 1) % kHistorySize;
  if (historyLen_ < kHistorySize) historyLen_++;

  // Noise floor: average of the OLDER samples (newest kFloorExclude skipped).
  const int floorWindow = historyLen_ - kFloorExclude;
  if (floorWindow < 10) return false;
  float sum = 0;
  // Oldest sample sits at historyHead_ when the ring is full, else at 0.
  const int start = historyLen_ < kHistorySize ? 0 : historyHead_;
  for (int i = 0; i < floorWindow; i++) {
    sum += history_[(start + i) % kHistorySize];
  }
  const float avg = sum / floorWindow;

  const uint32_t since = nowMs - lastBeatMs_;
  if (bass > avg * kThresholdMult && bass - prevBass > kMinRise &&
      bass > kMinLevel && since > kMinIntervalMs) {
    // Feed the tempo tracker with plausible onset gaps only.
    if (lastBeatMs_ != 0 && since >= kIntervalMinMs && since <= kIntervalMaxMs) {
      intervals_[intervalHead_] = (uint16_t)since;
      intervalHead_ = (intervalHead_ + 1) % kIntervals;
      if (intervalLen_ < kIntervals) intervalLen_++;
    }
    lastBeatMs_ = nowMs;
    envelope_ = 1.0f;
    return true;
  }
  return false;
}

void BeatDetector::decay(float dt) {
  envelope_ = std::fmax(0.0f, envelope_ - dt / kDecayS);
}

float BeatDetector::bpm(uint32_t nowMs) const {
  if (intervalLen_ < 4) return 0;                       // not enough evidence
  if (nowMs - lastBeatMs_ > kBpmStaleMs) return 0;      // music stopped
  // Median interval (insertion sort of at most 8 values).
  uint16_t sorted[kIntervals];
  for (int i = 0; i < intervalLen_; i++) {
    const uint16_t v = intervals_[i];
    int j = i;
    while (j > 0 && sorted[j - 1] > v) {
      sorted[j] = sorted[j - 1];
      j--;
    }
    sorted[j] = v;
  }
  const float medianMs = intervalLen_ % 2 == 1
                             ? sorted[intervalLen_ / 2]
                             : (sorted[intervalLen_ / 2 - 1] + sorted[intervalLen_ / 2]) * 0.5f;
  return 60000.0f / medianMs;
}

}  // namespace cube
