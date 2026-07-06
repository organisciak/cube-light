#include "cube_audio.h"

#include <cmath>

namespace cube {

void BeatDetector::reset() {
  historyLen_ = 0;
  historyHead_ = 0;
  lastBeatMs_ = 0;
  envelope_ = 0;
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
    lastBeatMs_ = nowMs;
    envelope_ = 1.0f;
    return true;
  }
  return false;
}

void BeatDetector::decay(float dt) {
  envelope_ = std::fmax(0.0f, envelope_ - dt / kDecayS);
}

}  // namespace cube
