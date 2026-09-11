#pragma once

// Microphone (PDM or standard I2S, chosen by CUBE_MIC_TYPE) -> AudioFrame.
//
// A FreeRTOS task on core 0 (Arduino runs on core 1) reads the mic,
// runs a 512-point FFT, collapses magnitudes into the 8 log-spaced bands the
// patterns expect, tracks an overall level, and feeds the shared
// BeatDetector. The render loop copies the latest frame with
// audioCaptureRead() once per frame.
//
// Tuning constants (squelch, gain decay) are first guesses to be refined on
// hardware — the stock WLED audioreactive config for this board used
// squelch=4 gain=60 with AGC, so expect to iterate.

#include "cube_pattern.h"

namespace cube {

/** Start the capture task. Returns false if the I2S driver failed. */
bool audioCaptureStart();

/** True once capture is running. False on boards built with no mic
 * (CUBE_MIC_TYPE=0) or when the driver failed — the console shows "no
 * microphone" and the playlist skips music-reactive presets. */
bool audioCaptureAvailable();

/** "pdm", "i2s" or "none" — the compiled-in mic type, for diagnostics. */
const char* audioCaptureKind();

/** Copy the most recent audio frame (level, bands, beat envelope). */
void audioCaptureRead(AudioFrame& out);

/** Diagnostics for /api/audio — enough to tell "no data" from "quiet". */
struct AudioStats {
  uint32_t frames;    // FFT frames processed since boot (~43/s when healthy)
  float lastRms;      // raw-sample RMS of the latest frame (pre-squelch)
  float lastDc;       // DC offset of the latest frame
  int16_t rawMin, rawMax;  // sample extremes of the latest frame
};
void audioCaptureStats(AudioStats& out);

/** Reinstall the I2S driver with a different channel format (left/right)
 * and/or squelch. Runtime knobs for on-device mic bring-up. */
void audioCaptureReconfigure(bool channelLeft, float squelchRms);

}  // namespace cube
