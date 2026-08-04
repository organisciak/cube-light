#pragma once
#include <cstdint>

// Shared 10x10 image store for the image-3d pattern (album art, etc.).
// Pure C++ so the native harness can exercise it; upload transport and
// persistence live in firmware/src. Source images are box-averaged down to
// the face size on set, so callers can push anything up to 64x64 RGB24.

namespace cube {

constexpr int IMG_N = 10;                 // face size (matches CUBE_N)
constexpr int IMG_MAX_SRC = 64;           // largest accepted source dimension

/**
 * Set the image from a w*h RGB24 buffer (row-major, top row first).
 * Box-averages down to 10x10 (nearest-cell when the source is smaller).
 * Returns false on bad dimensions.
 */
bool cubeImageSet(const uint8_t* rgb, int w, int h);

/** True once an image has been set (false = showing the built-in gradient). */
bool cubeImageLoaded();

/** 10*10*3 pixels, row-major from the top row. Never null. */
const uint8_t* cubeImagePixels();

/** Average color of the current image (drives the background glow). */
void cubeImageAverage(uint8_t out[3]);

}  // namespace cube
