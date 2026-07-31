// Native host harness: runs the exact firmware pattern core on the dev
// machine and streams frames as WLED DNRGB packets over UDP. Point it at a
// real WLED cube, or at the Node dev server running with VIRTUAL_WLED=1 to
// see the C++ engine's output in the browser's 3D preview.
//
// Usage:
//   ./cube-native [patternId] [key=value ...] [options]
// Options:
//   --host H       destination (default 127.0.0.1)
//   --port P       destination UDP port (default 21324)
//   --fps N        frame rate (default 30)
//   --seconds S    exit after S seconds (default: run forever)
//   --fake-audio   synthesize level/bands/beat so audio-reactive params move
// Params: bare key=value pairs; "true"/"false" parse as bool, numerics as
// float, anything else as string. E.g.:
//   ./cube-native wavy-sheet palette=arctic amp=2 --fake-audio

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "cube_pattern.h"

using namespace cube;

namespace {

constexpr int kMaxLedsPerPacket = 489;  // header 4B + 489*3B fits one MTU
constexpr uint8_t kDnrgbOpcode = 0x04;
constexpr uint8_t kTimeoutSecs = 2;

double nowSeconds() {
  timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

void sendFrame(int sock, const sockaddr_in& dst, const uint8_t* rgb) {
  uint8_t pkt[4 + kMaxLedsPerPacket * 3];
  for (int start = 0; start < NUM_LEDS; start += kMaxLedsPerPacket) {
    const int count = std::min(kMaxLedsPerPacket, NUM_LEDS - start);
    pkt[0] = kDnrgbOpcode;
    pkt[1] = kTimeoutSecs;
    pkt[2] = (uint8_t)(start >> 8);
    pkt[3] = (uint8_t)(start & 0xff);
    std::memcpy(pkt + 4, rgb + start * 3, count * 3);
    sendto(sock, pkt, 4 + count * 3, 0, (const sockaddr*)&dst, sizeof(dst));
  }
}

void fakeAudio(AudioFrame& audio, double t) {
  // A plausible synthetic groove: slow level swell, per-band wobble, and a
  // 120 BPM beat envelope with exponential decay.
  audio.level = 0.35f + 0.3f * (float)std::sin(t * 0.7);
  for (int i = 0; i < AUDIO_BANDS; i++) {
    audio.bands[i] =
        0.5f + 0.5f * (float)std::sin(t * (1.1 + i * 0.37) + i * 1.7);
  }
  const double beatPeriod = 0.5;  // 120 BPM
  const double sinceBeat = std::fmod(t, beatPeriod);
  audio.beat = (float)std::exp(-sinceBeat * 6.0);
  audio.bpm = 120.0f;
}

}  // namespace

int main(int argc, char** argv) {
  const char* patternId = kDefaultPatternId;
  const char* host = "127.0.0.1";
  int port = 21324;
  double fps = 30;
  double seconds = -1;
  bool useFakeAudio = false;
  Params params;

  for (int i = 1; i < argc; i++) {
    const char* a = argv[i];
    if (std::strcmp(a, "--host") == 0 && i + 1 < argc) {
      host = argv[++i];
    } else if (std::strcmp(a, "--port") == 0 && i + 1 < argc) {
      port = std::atoi(argv[++i]);
    } else if (std::strcmp(a, "--fps") == 0 && i + 1 < argc) {
      fps = std::atof(argv[++i]);
    } else if (std::strcmp(a, "--seconds") == 0 && i + 1 < argc) {
      seconds = std::atof(argv[++i]);
    } else if (std::strcmp(a, "--fake-audio") == 0) {
      useFakeAudio = true;
    } else if (std::strchr(a, '=')) {
      char key[64];
      const char* eq = std::strchr(a, '=');
      const size_t klen = std::min((size_t)(eq - a), sizeof(key) - 1);
      std::memcpy(key, a, klen);
      key[klen] = '\0';
      const char* val = eq + 1;
      char* end = nullptr;
      const float f = std::strtof(val, &end);
      if (std::strcmp(val, "true") == 0) {
        params.setBool(key, true);
      } else if (std::strcmp(val, "false") == 0) {
        params.setBool(key, false);
      } else if (end && *end == '\0' && end != val) {
        params.setNum(key, f);
      } else {
        params.setStr(key, val);
      }
    } else {
      patternId = a;
    }
  }

  const Pattern* pat = findPattern(patternId);
  if (!pat) {
    std::fprintf(stderr, "unknown pattern '%s'. available:", patternId);
    for (int i = 0; i < kPatternCount; i++) std::fprintf(stderr, " %s", kPatterns[i]->id);
    std::fprintf(stderr, "\n");
    return 1;
  }

  const int sock = socket(AF_INET, SOCK_DGRAM, 0);
  if (sock < 0) {
    std::perror("socket");
    return 1;
  }
  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons((uint16_t)port);
  if (inet_pton(AF_INET, host, &dst.sin_addr) != 1) {
    std::fprintf(stderr, "invalid host '%s' (use a literal IP)\n", host);
    return 1;
  }

  static uint8_t buffer[NUM_LEDS * 3];
  Geometry geo;  // default layout — must match the previewing server's layout
  AudioFrame audio;
  PatternCtx ctx{buffer, &geo, 0, 0, &audio, &params};

  std::printf("cube-native: pattern=%s -> %s:%d @ %.0ffps%s\n", pat->id, host,
              port, fps, useFakeAudio ? " (fake audio)" : "");

  const double t0 = nowSeconds();
  double lastT = 0;
  long frames = 0;
  if (pat->init) pat->init(ctx);

  while (true) {
    const double t = nowSeconds() - t0;
    if (seconds > 0 && t >= seconds) break;
    ctx.t = (float)t;
    ctx.dt = (float)(t - lastT);
    lastT = t;
    if (useFakeAudio) fakeAudio(audio, t);
    pat->render(ctx);
    sendFrame(sock, dst, buffer);
    frames++;

    const double nextT = t0 + frames * (1.0 / fps);
    const double sleepS = nextT - nowSeconds();
    if (sleepS > 0) usleep((useconds_t)(sleepS * 1e6));
  }

  std::printf("cube-native: sent %ld frames (%.1fs)\n", frames, nowSeconds() - t0);
  close(sock);
  return 0;
}
