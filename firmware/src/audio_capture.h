#pragma once

// On-board PDM microphone -> AudioFrame pipeline.
//
// A FreeRTOS task on core 0 (Arduino runs on core 1) reads the I2S PDM mic,
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

/** Copy the most recent audio frame (level, bands, beat envelope). */
void audioCaptureRead(AudioFrame& out);

}  // namespace cube
