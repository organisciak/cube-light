#include "audio_capture.h"

#include <Arduino.h>
#include <arduinoFFT.h>
#include <driver/i2s_pdm.h>

#include "cube_audio.h"

#ifndef CUBE_MIC_DATA_PIN
#define CUBE_MIC_DATA_PIN 32  // 618WL PDM mic data (from stock WLED cfg)
#endif
#ifndef CUBE_MIC_CLK_PIN
#define CUBE_MIC_CLK_PIN 15  // 618WL PDM mic clock
#endif

namespace cube {
namespace {

// IDF 5.x: PDM RX uses the dedicated i2s_pdm channel API. The legacy
// driver/i2s.h path delivers all-zero samples on this core (clock runs,
// data never latches) — diagnosed on hardware via /api/audio.
constexpr int kSampleRate = 22050;
constexpr int kSamples = 512;  // -> 256 bins, ~43Hz each, ~23ms per frame

float vReal[kSamples];
float vImag[kSamples];

portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
AudioFrame s_frame;
BeatDetector s_beat;

float s_bandPeak[AUDIO_BANDS];
float s_bandFloor[AUDIO_BANDS];  // per-band running noise floor
float s_bandOut[AUDIO_BANDS];    // attack/release-smoothed outputs
float s_levelPeak = 0.01f;
float s_levelOut = 0;
constexpr float kPeakDecay = 0.9995f;  // per FFT frame (~43Hz)
constexpr float kAttack = 0.55f;   // per-frame smoothing when rising
constexpr float kRelease = 0.10f;  // per-frame smoothing when falling
float s_squelchRms = 60.0f;
bool s_channelLeft = false;
volatile bool s_reconfigPending = false;
AudioStats s_stats = {0, 0, 0, 0, 0};

i2s_chan_handle_t s_rx = nullptr;

bool initPdm() {
  i2s_chan_config_t chanCfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chanCfg, nullptr, &s_rx) != ESP_OK) return false;

  i2s_pdm_rx_config_t cfg = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(kSampleRate),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                 I2S_SLOT_MODE_MONO),
      .gpio_cfg =
          {
              .clk = (gpio_num_t)CUBE_MIC_CLK_PIN,
              .din = (gpio_num_t)CUBE_MIC_DATA_PIN,
              .invert_flags = {.clk_inv = false},
          },
  };
  // Which PDM half-cycle the mic drives; runtime-switchable via /api/miccfg.
  cfg.slot_cfg.slot_mask = s_channelLeft ? I2S_PDM_SLOT_LEFT : I2S_PDM_SLOT_RIGHT;

  if (i2s_channel_init_pdm_rx_mode(s_rx, &cfg) != ESP_OK) return false;
  return i2s_channel_enable(s_rx) == ESP_OK;
}

void teardownPdm() {
  if (!s_rx) return;
  i2s_channel_disable(s_rx);
  i2s_del_channel(s_rx);
  s_rx = nullptr;
}

void captureTask(void*) {
  static int16_t raw[kSamples];
  static ArduinoFFT<float> FFT(vReal, vImag, kSamples, (float)kSampleRate);
  vTaskDelay(pdMS_TO_TICKS(500));  // let WiFi/net bring-up settle first
  for (;;) {
    // Reconfiguration happens here, in task context, so the channel is never
    // deleted underneath a blocking read.
    if (s_reconfigPending) {
      s_reconfigPending = false;
      teardownPdm();
      if (!initPdm()) {
        Serial.println("[mic] pdm reinit failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
        continue;
      }
      Serial.printf("[mic] reconfigured: channel=%s squelch=%.0f\n",
                    s_channelLeft ? "left" : "right", s_squelchRms);
    }
    size_t bytesRead = 0;
    if (i2s_channel_read(s_rx, raw, sizeof(raw), &bytesRead, pdMS_TO_TICKS(500)) !=
        ESP_OK)
      continue;
    const int n = bytesRead / 2;
    if (n < kSamples) continue;

    // DC removal + window into the FFT buffers; RMS for the level/squelch.
    float mean = 0;
    for (int i = 0; i < kSamples; i++) mean += raw[i];
    mean /= kSamples;
    float sumSq = 0;
    for (int i = 0; i < kSamples; i++) {
      const float v = raw[i] - mean;
      vReal[i] = v;
      vImag[i] = 0;
      sumSq += v * v;
    }
    const float rms = sqrtf(sumSq / kSamples);
    // Soft-knee squelch: full gate below squelch/2, fully open at squelch.
    // The old hard gate snapped level/bands to zero the instant RMS dipped
    // under the threshold — visible as stutter in audio-reactive patterns.
    const float knee = 0.5f * s_squelchRms;
    const float gate =
        knee <= 0 ? 1.0f : min(1.0f, max(0.0f, (rms - knee) / (knee + 1e-3f)));
    int16_t mn = raw[0], mx = raw[0];
    for (int i = 1; i < kSamples; i++) {
      if (raw[i] < mn) mn = raw[i];
      if (raw[i] > mx) mx = raw[i];
    }
    s_stats.frames++;
    s_stats.lastRms = rms;
    s_stats.lastDc = mean;
    s_stats.rawMin = mn;
    s_stats.rawMax = mx;

    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();

    // 8 log-spaced bands: bins [2^b, 2^(b+1)) starting at bin 1
    // (~43-86Hz for band 0 ... ~5.5-11kHz for band 7).
    float bassTarget = 0;
    for (int b = 0; b < AUDIO_BANDS; b++) {
      const int lo = 1 << b;
      const int hi = min(kSamples / 2, 1 << (b + 1));
      float acc = 0;
      for (int i = lo; i < hi; i++) acc += vReal[i];
      const float mag = acc / (hi - lo);
      // Per-band noise floor (snaps down, creeps up) subtracted before
      // normalizing against the per-band peak. Without this, every band
      // rides the same overall loudness envelope and the spectrum reads as
      // one mass instead of independent bars.
      s_bandFloor[b] = mag < s_bandFloor[b] ? mag : s_bandFloor[b] * 1.002f + 0.05f;
      s_bandPeak[b] = max(mag, max(s_bandPeak[b] * kPeakDecay, s_bandFloor[b] + 1.0f));
      const float norm = min(
          1.0f, max(0.0f, (mag - s_bandFloor[b]) /
                              (s_bandPeak[b] - s_bandFloor[b] + 1e-3f)));
      const float target = norm * gate;
      if (b == 0) bassTarget = target;
      s_bandOut[b] += (target - s_bandOut[b]) * (target > s_bandOut[b] ? kAttack : kRelease);
    }
    s_levelPeak = max(rms, s_levelPeak * kPeakDecay);
    const float levelTarget = min(1.0f, rms / s_levelPeak) * gate;
    s_levelOut += (levelTarget - s_levelOut) * (levelTarget > s_levelOut ? kAttack : kRelease);

    // Beat detection wants the sharp pre-smoothing transient.
    s_beat.onFrame(bassTarget, millis());
    s_beat.decay((float)kSamples / kSampleRate);

    portENTER_CRITICAL(&s_mux);
    s_frame.level = s_levelOut;
    for (int b = 0; b < AUDIO_BANDS; b++) s_frame.bands[b] = s_bandOut[b];
    s_frame.beat = s_beat.envelope();
    portEXIT_CRITICAL(&s_mux);
  }
}

}  // namespace

bool audioCaptureStart() {
#ifdef CUBE_MIC_BISECT_NO_I2S
  Serial.println("[mic] BISECT: mic fully disabled");
  return false;
#endif
  for (int b = 0; b < AUDIO_BANDS; b++) {
    s_bandPeak[b] = 1.0f;
    s_bandFloor[b] = 1e9f;  // first frame snaps it to reality
    s_bandOut[b] = 0;
  }
  if (!initPdm()) {
    Serial.println("[mic] pdm init failed");
    return false;
  }
  xTaskCreatePinnedToCore(captureTask, "mic", 8192, nullptr, 1, nullptr, 0);
  Serial.printf("[mic] pdm capture on data=%d clk=%d (new i2s_pdm driver)\n",
                CUBE_MIC_DATA_PIN, CUBE_MIC_CLK_PIN);
  return true;
}

void audioCaptureRead(AudioFrame& out) {
  portENTER_CRITICAL(&s_mux);
  out = s_frame;
  portEXIT_CRITICAL(&s_mux);
}

void audioCaptureStats(AudioStats& out) {
  portENTER_CRITICAL(&s_mux);
  out = s_stats;
  portEXIT_CRITICAL(&s_mux);
}

void audioCaptureReconfigure(bool channelLeft, float squelchRms) {
  s_squelchRms = squelchRms;
  if (channelLeft != s_channelLeft) {
    s_channelLeft = channelLeft;
    s_reconfigPending = true;  // applied by the capture task between reads
  }
}

}  // namespace cube
