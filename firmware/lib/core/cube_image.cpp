#include "cube_image.h"

namespace cube {
namespace {

uint8_t s_img[IMG_N * IMG_N * 3];
uint8_t s_avg[3] = {0, 0, 0};
bool s_loaded = false;
bool s_placeholder = false;

void computeAverage() {
  uint32_t sum[3] = {0, 0, 0};
  for (int i = 0; i < IMG_N * IMG_N; i++) {
    sum[0] += s_img[i * 3];
    sum[1] += s_img[i * 3 + 1];
    sum[2] += s_img[i * 3 + 2];
  }
  for (int c = 0; c < 3; c++) s_avg[c] = (uint8_t)(sum[c] / (IMG_N * IMG_N));
}

// Until something is uploaded: a soft diagonal sunset gradient, so the
// pattern shows *something* recognizable rather than a black cube.
void makePlaceholder() {
  for (int y = 0; y < IMG_N; y++) {
    for (int x = 0; x < IMG_N; x++) {
      const float t = (float)(x + y) / (2 * (IMG_N - 1));
      uint8_t* px = &s_img[(y * IMG_N + x) * 3];
      px[0] = (uint8_t)(40 + 200 * t);
      px[1] = (uint8_t)(20 + 60 * t);
      px[2] = (uint8_t)(120 - 90 * t);
    }
  }
  computeAverage();
  s_placeholder = true;
}

void ensureInit() {
  if (!s_loaded && !s_placeholder) makePlaceholder();
}

}  // namespace

bool cubeImageSet(const uint8_t* rgb, int w, int h) {
  if (!rgb || w < 1 || h < 1 || w > IMG_MAX_SRC || h > IMG_MAX_SRC) return false;
  for (int ty = 0; ty < IMG_N; ty++) {
    // Source rect for this target cell; at least 1px so upscales sample
    // nearest instead of averaging an empty box.
    const int y0 = ty * h / IMG_N;
    const int y1 = (ty + 1) * h / IMG_N > y0 ? (ty + 1) * h / IMG_N : y0 + 1;
    for (int tx = 0; tx < IMG_N; tx++) {
      const int x0 = tx * w / IMG_N;
      const int x1 = (tx + 1) * w / IMG_N > x0 ? (tx + 1) * w / IMG_N : x0 + 1;
      uint32_t sum[3] = {0, 0, 0};
      for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
          const uint8_t* px = &rgb[(y * w + x) * 3];
          sum[0] += px[0];
          sum[1] += px[1];
          sum[2] += px[2];
        }
      }
      const uint32_t n = (uint32_t)((y1 - y0) * (x1 - x0));
      uint8_t* out = &s_img[(ty * IMG_N + tx) * 3];
      out[0] = (uint8_t)(sum[0] / n);
      out[1] = (uint8_t)(sum[1] / n);
      out[2] = (uint8_t)(sum[2] / n);
    }
  }
  computeAverage();
  s_loaded = true;
  return true;
}

bool cubeImageLoaded() { return s_loaded; }

const uint8_t* cubeImagePixels() {
  ensureInit();
  return s_img;
}

void cubeImageAverage(uint8_t out[3]) {
  ensureInit();
  out[0] = s_avg[0];
  out[1] = s_avg[1];
  out[2] = s_avg[2];
}

}  // namespace cube
