#include "audio_capture.h"

#include <Arduino.h>
#include <arduinoFFT.h>
#include <driver/i2s.h>

#include "cube_audio.h"

#ifndef CUBE_MIC_DATA_PIN
#define CUBE_MIC_DATA_PIN 32  // 618WL PDM mic data (from stock WLED cfg)
#endif
#ifndef CUBE_MIC_CLK_PIN
#define CUBE_MIC_CLK_PIN 15  // 618WL PDM mic clock
#endif

namespace cube {
namespace {

constexpr i2s_port_t kPort = I2S_NUM_0;  // PDM RX exists only on I2S0
constexpr int kSampleRate = 22050;
constexpr int kSamples = 512;  // -> 256 bins, ~43Hz each, ~23ms per frame

float vReal[kSamples];
float vImag[kSamples];

// Shared with the render loop; guarded by a spinlock kept only for the copy.
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
AudioFrame s_frame;
BeatDetector s_beat;

// Per-band adaptive normalization: running peak with slow decay so band
// output spans 0..1 across quiet rooms and loud camps alike.
float s_bandPeak[AUDIO_BANDS];
float s_levelPeak = 0.01f;
constexpr float kPeakDecay = 0.9995f;   // per FFT frame (~43Hz)
constexpr float kSquelchRms = 60.0f;    // raw-sample RMS below this = silence

bool initI2s() {
  const i2s_config_t cfg = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
      .sample_rate = kSampleRate,
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      // If the mic reads silent on hardware, the first thing to try is
      // ONLY_LEFT here — PDM mics differ on which edge they drive.
      .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 4,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = false,
      .fixed_mclk = 0,
  };
  if (i2s_driver_install(kPort, &cfg, 0, nullptr) != ESP_OK) return false;
  const i2s_pin_config_t pins = {
      .mck_io_num = I2S_PIN_NO_CHANGE,
      .bck_io_num = I2S_PIN_NO_CHANGE,
      .ws_io_num = CUBE_MIC_CLK_PIN,
      .data_out_num = I2S_PIN_NO_CHANGE,
      .data_in_num = CUBE_MIC_DATA_PIN,
  };
  return i2s_set_pin(kPort, &pins) == ESP_OK;
}

void captureTask(void*) {
  static int16_t raw[kSamples];
  static ArduinoFFT<float> FFT(vReal, vImag, kSamples, (float)kSampleRate);
  vTaskDelay(pdMS_TO_TICKS(500));  // let WiFi/net bring-up settle first
  for (;;) {
    size_t bytesRead = 0;
    i2s_read(kPort, raw, sizeof(raw), &bytesRead, portMAX_DELAY);
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
    const bool silent = rms < kSquelchRms;

    FFT.windowing(FFTWindow::Hamming, FFTDirection::Forward);
    FFT.compute(FFTDirection::Forward);
    FFT.complexToMagnitude();

    // 8 log-spaced bands: bins [2^b, 2^(b+1)) starting at bin 1
    // (~43-86Hz for band 0 ... ~5.5-11kHz for band 7).
    float bands[AUDIO_BANDS];
    for (int b = 0; b < AUDIO_BANDS; b++) {
      const int lo = 1 << b;
      const int hi = min(kSamples / 2, 1 << (b + 1));
      float acc = 0;
      for (int i = lo; i < hi; i++) acc += vReal[i];
      const float mag = acc / (hi - lo);
      s_bandPeak[b] = max(mag, max(s_bandPeak[b] * kPeakDecay, 1.0f));
      bands[b] = silent ? 0.0f : min(1.0f, mag / s_bandPeak[b]);
    }
    s_levelPeak = max(rms, s_levelPeak * kPeakDecay);
    const float level = silent ? 0.0f : min(1.0f, rms / s_levelPeak);

    s_beat.onFrame(bands[0], millis());
    s_beat.decay((float)kSamples / kSampleRate);

    portENTER_CRITICAL(&s_mux);
    s_frame.level = level;
    for (int b = 0; b < AUDIO_BANDS; b++) s_frame.bands[b] = bands[b];
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
  for (int b = 0; b < AUDIO_BANDS; b++) s_bandPeak[b] = 1.0f;
  if (!initI2s()) {
    Serial.println("[mic] i2s init failed");
    return false;
  }
#ifdef CUBE_MIC_BISECT_NO_TASK
  Serial.println("[mic] BISECT: i2s installed, task NOT started");
#else
  xTaskCreatePinnedToCore(captureTask, "mic", 8192, nullptr, 1, nullptr, 0);
#endif
  Serial.printf("[mic] pdm capture on data=%d clk=%d\n", CUBE_MIC_DATA_PIN,
                CUBE_MIC_CLK_PIN);
  return true;
}

void audioCaptureRead(AudioFrame& out) {
  portENTER_CRITICAL(&s_mux);
  out = s_frame;
  portEXIT_CRITICAL(&s_mux);
}

}  // namespace cube
