// Port of src/shared/patterns/snake3d.ts — game, hint overlay, and the BFS
// auto-solver. Segment list is a fixed array (the snake can't exceed the
// cube's 1000 cells); BFS scratch buffers are static.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"
#include "cube_snake.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;
constexpr int N3 = NUM_LEDS;

const int STEP[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};
inline SnakeDir opposite(SnakeDir d) {
  return (SnakeDir)((int)d ^ 1);  // XP<->XN, YP<->YN, ZP<->ZN
}

struct Segment {
  int8_t x, y, z;
};

struct SnakeGame {
  Segment segments[N3];  // head first
  int length;
  SnakeDir direction;
  SnakeDir pendingDir;
  bool hasPendingDir;
  Segment apple;
  float lastTickT;
  bool alive;
  bool started;  // false until first input; snake idles while false
  float deathStartT;
  int score;
  int highScore;
  SnakeDir hintDir;
  bool hasHint;
  float hintStartT;
};

SnakeGame s_game;
bool s_gameInit = false;

constexpr int kInputQueueMax = 8;
SnakeDir s_inputQueue[kInputQueueMax];
int s_inputCount = 0;
SnakeDir s_pendingHint;
bool s_hasPendingHint = false;

inline int cellIdx(int x, int y, int z) { return x + y * N + z * N * N; }

Segment randomEmpty() {
  static uint8_t occ[N3];
  std::memset(occ, 0, sizeof(occ));
  for (int i = 0; i < s_game.length; i++) {
    occ[cellIdx(s_game.segments[i].x, s_game.segments[i].y, s_game.segments[i].z)] = 1;
  }
  for (int attempt = 0; attempt < 2000; attempt++) {
    const int x = (int)(frand() * N);
    const int y = (int)(frand() * N);
    const int z = (int)(frand() * N);
    if (!occ[cellIdx(x, y, z)]) return {(int8_t)x, (int8_t)y, (int8_t)z};
  }
  return {0, 0, 0};
}

void reset(int prevHighScore) {
  const int8_t c = N / 2;
  s_game.length = 3;
  s_game.segments[0] = {c, c, c};
  s_game.segments[1] = {(int8_t)(c - 1), c, c};
  s_game.segments[2] = {(int8_t)(c - 2), c, c};
  s_game.direction = SnakeDir::XP;
  s_game.hasPendingDir = false;
  s_game.lastTickT = 0;
  s_game.alive = true;
  s_game.started = false;
  s_game.deathStartT = 0;
  s_game.score = 0;
  s_game.highScore = prevHighScore;
  s_game.hasHint = false;
  s_game.hintStartT = -999;
  s_game.apple = randomEmpty();
  s_inputCount = 0;
  s_hasPendingHint = false;
  s_gameInit = true;
}

float speedHz(int score, float base, float max, float lengthForMax) {
  const float t = std::fmin(1.0f, score / std::fmax(1.0f, lengthForMax));
  return base + (max - base) * t;
}

// ---- BFS solver ------------------------------------------------------------

constexpr int kUnreachable = 0x7fff;

// Shortest-path distance start -> target through unblocked cells; the target
// itself is passable. kUnreachable if no path.
int bfsDist(const uint8_t* occ, const Segment& start, const Segment& target) {
  static int16_t dist[N3];
  static uint16_t queue[N3];
  const int sIdx = cellIdx(start.x, start.y, start.z);
  const int tIdx = cellIdx(target.x, target.y, target.z);
  if (sIdx == tIdx) return 0;
  std::memset(dist, -1, sizeof(dist));
  dist[sIdx] = 0;
  int qHead = 0, qTail = 0;
  queue[qTail++] = (uint16_t)sIdx;
  while (qHead < qTail) {
    const int cur = queue[qHead++];
    const int cx = cur % N;
    const int cy = (cur / N) % N;
    const int cz = cur / (N * N);
    const int cd = dist[cur];
    for (int d = 0; d < 6; d++) {
      const int nx = cx + STEP[d][0], ny = cy + STEP[d][1], nz = cz + STEP[d][2];
      if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
      const int ni = cellIdx(nx, ny, nz);
      if (dist[ni] != -1) continue;
      if (ni == tIdx) return cd + 1;
      if (occ[ni]) continue;
      dist[ni] = (int16_t)(cd + 1);
      queue[qTail++] = (uint16_t)ni;
    }
  }
  return kUnreachable;
}

// Count empty cells reachable from start (start counts as 1).
int reachableCount(const uint8_t* occ, const Segment& start) {
  static uint8_t seen[N3];
  static uint16_t queue[N3];
  std::memset(seen, 0, sizeof(seen));
  const int sIdx = cellIdx(start.x, start.y, start.z);
  seen[sIdx] = 1;
  int count = 1;
  int qHead = 0, qTail = 0;
  queue[qTail++] = (uint16_t)sIdx;
  while (qHead < qTail) {
    const int cur = queue[qHead++];
    const int cx = cur % N;
    const int cy = (cur / N) % N;
    const int cz = cur / (N * N);
    for (int d = 0; d < 6; d++) {
      const int nx = cx + STEP[d][0], ny = cy + STEP[d][1], nz = cz + STEP[d][2];
      if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
      const int ni = cellIdx(nx, ny, nz);
      if (seen[ni] || occ[ni]) continue;
      seen[ni] = 1;
      count++;
      queue[qTail++] = (uint16_t)ni;
    }
  }
  return count;
}

struct AutoOption {
  SnakeDir dir;
  Segment newHead;
  bool willEat;
  int distToApple;
  bool safe;
  int space;
};

/**
 * Strategy (same as the TS solver):
 *  1. Enumerate legal moves (in-bounds, not reverse, no immediate body hit).
 *  2. A move is "safe" if the post-move head can still reach the post-move
 *     tail tip — the classical tail-following guarantee.
 *  3. Prefer the safe move with the shortest path to the apple (ties: more
 *     reachable space).
 *  4. nearMiss chance to swerve off an eating move when a safe non-eating
 *     alternative exists.
 *  5. No safe moves: maximize reachable space; ties: closer to the tail.
 */
SnakeDir autoChoose(float nearMiss) {
  const SnakeGame& g = s_game;
  const Segment head = g.segments[0];
  const Segment apple = g.apple;
  const Segment tail = g.segments[g.length - 1];

  static uint8_t occWithoutHead[N3];
  static uint8_t occForTailCheck[N3];

  AutoOption options[6];
  int optionCount = 0;

  for (int d = 0; d < 6; d++) {
    if ((SnakeDir)d == opposite(g.direction)) continue;
    const int nx = head.x + STEP[d][0];
    const int ny = head.y + STEP[d][1];
    const int nz = head.z + STEP[d][2];
    if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;

    const bool willEat = nx == apple.x && ny == apple.y && nz == apple.z;
    const Segment newHead = {(int8_t)nx, (int8_t)ny, (int8_t)nz};
    // Post-move body: newHead + old body, dropping the old tail tip unless
    // eating. newBodyLen counts segments including the new head.
    const int keep = willEat ? g.length : g.length - 1;

    // Immediate self-collision: does the new head land on a kept body cell?
    bool hitsSelf = false;
    for (int i = 0; i < keep; i++) {
      if (g.segments[i].x == nx && g.segments[i].y == ny && g.segments[i].z == nz) {
        hitsSelf = true;
        break;
      }
    }
    if (hitsSelf) continue;

    // occWithoutHead: all kept body cells (obstacles for pathing).
    std::memset(occWithoutHead, 0, sizeof(occWithoutHead));
    for (int i = 0; i < keep; i++) {
      occWithoutHead[cellIdx(g.segments[i].x, g.segments[i].y, g.segments[i].z)] = 1;
    }

    const int distToApple = willEat ? 0 : bfsDist(occWithoutHead, newHead, apple);

    // Tail-reachability check: obstacles exclude the new tail tip itself.
    const Segment newTail = g.segments[keep - 1];
    std::memset(occForTailCheck, 0, sizeof(occForTailCheck));
    for (int i = 0; i < keep - 1; i++) {
      occForTailCheck[cellIdx(g.segments[i].x, g.segments[i].y, g.segments[i].z)] = 1;
    }
    const bool safe = bfsDist(occForTailCheck, newHead, newTail) != kUnreachable;
    const int space = reachableCount(occWithoutHead, newHead);

    options[optionCount++] = {(SnakeDir)d, newHead, willEat, distToApple, safe, space};
  }

  if (optionCount == 0) return g.direction;  // doomed; end gracefully

  // Best safe option: shortest distToApple, ties by larger space.
  int best = -1;
  for (int i = 0; i < optionCount; i++) {
    if (!options[i].safe) continue;
    if (best < 0 || options[i].distToApple < options[best].distToApple ||
        (options[i].distToApple == options[best].distToApple &&
         options[i].space > options[best].space)) {
      best = i;
    }
  }
  if (best >= 0) {
    if (options[best].willEat && nearMiss > 0 && frand() < nearMiss) {
      int swerve = -1;
      for (int i = 0; i < optionCount; i++) {
        if (!options[i].safe || options[i].willEat || options[i].distToApple == kUnreachable)
          continue;
        if (swerve < 0 || options[i].distToApple < options[swerve].distToApple) swerve = i;
      }
      if (swerve >= 0) return options[swerve].dir;
    }
    return options[best].dir;
  }

  // Survival fallback: max space; ties by Manhattan distance to the tail.
  best = 0;
  for (int i = 1; i < optionCount; i++) {
    if (options[i].space > options[best].space) {
      best = i;
    } else if (options[i].space == options[best].space) {
      const auto manhattan = [&](const AutoOption& o) {
        return std::abs(o.newHead.x - tail.x) + std::abs(o.newHead.y - tail.y) +
               std::abs(o.newHead.z - tail.z);
      };
      if (manhattan(options[i]) < manhattan(options[best])) best = i;
    }
  }
  return options[best].dir;
}

// ---- hint overlay ----------------------------------------------------------

// 3x3 patch on the far face of the pressed axis, centered on the head's
// other-axis coords. Axis colors match the UI: x=red, y=green, z=blue.
void drawDirectionHint(PatternCtx& ctx, SnakeDir dir, const Segment& head, float intensity) {
  uint8_t* buffer = ctx.buffer;
  const int axis = (int)dir / 2;      // 0=x 1=y 2=z
  const int sign = ((int)dir % 2 == 0) ? 1 : -1;
  const int fixed = sign > 0 ? N - 1 : 0;
  const int color[3][3] = {{255, 60, 60}, {70, 230, 70}, {90, 140, 255}};
  const int* col = color[axis];

  const int ca = axis == 0 ? head.y : head.x;
  const int cb = axis == 2 ? head.y : head.z;
  for (int a = std::max(0, ca - 1); a <= std::min(N - 1, ca + 1); a++) {
    for (int b = std::max(0, cb - 1); b <= std::min(N - 1, cb + 1); b++) {
      int x, y, z;
      if (axis == 0) { x = fixed; y = a; z = b; }
      else if (axis == 1) { x = a; y = fixed; z = b; }
      else { x = a; y = b; z = fixed; }
      const int i = ctx.idx(x, y, z) * 3;
      buffer[i] = (uint8_t)std::fmin(255.0f, buffer[i] + std::lround(col[0] * intensity));
      buffer[i + 1] = (uint8_t)std::fmin(255.0f, buffer[i + 1] + std::lround(col[1] * intensity));
      buffer[i + 2] = (uint8_t)std::fmin(255.0f, buffer[i + 2] + std::lround(col[2] * intensity));
    }
  }
}

// ---- pattern ---------------------------------------------------------------

void init(PatternCtx& ctx) {
  reset(s_gameInit ? s_game.highScore : 0);
  s_game.lastTickT = ctx.t;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  if (!s_gameInit) reset(0);
  SnakeGame& g = s_game;
  const Params& p = *ctx.params;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;

  const bool autoMode = p.str("mode", "auto")[0] == 'a';
  const float nearMiss = clamp01(p.num("nearMiss", 0.0f));
  const float baseSpeed = p.num("baseSpeed", 4.0f);
  const float maxSpeed = p.num("maxSpeed", 12.0f);
  const float lengthForMax = p.num("lengthForMax", 25.0f);
  const PaletteRef pal = resolvePalette(p.str("palette", "spectrum"));
  const float appleR = p.num("appleR", 255.0f);
  const float appleG = p.num("appleG", 40.0f);
  const float appleB = p.num("appleB", 40.0f);
  const float hintFullScore = p.num("hintFullScore", 3.0f);
  const float hintGoneScore = std::fmax(hintFullScore + 1, p.num("hintGoneScore", 8.0f));
  const float hintFlashSec = std::fmax(0.05f, p.num("hintFlashSec", 0.45f));

  // Auto mode starts moving immediately, no first-input gate.
  if (autoMode && !g.started && g.alive) g.started = true;

  // Stamp incoming hint with game time (queueSnakeInput can't see ctx.t).
  if (s_hasPendingHint) {
    g.hintDir = s_pendingHint;
    g.hasHint = true;
    g.hintStartT = t;
    s_hasPendingHint = false;
  }

  // At most one queued input per tick.
  if (!g.hasPendingDir && s_inputCount > 0) {
    const SnakeDir next = s_inputQueue[0];
    s_inputCount--;
    std::memmove(s_inputQueue, s_inputQueue + 1, s_inputCount * sizeof(SnakeDir));
    if (next != opposite(g.direction)) {
      g.pendingDir = next;
      g.hasPendingDir = true;
    }
  }

  if (g.alive && g.started) {
    const float speed = speedHz(g.score, baseSpeed, maxSpeed, lengthForMax);
    const float tickInterval = 1.0f / std::fmax(0.5f, speed);

    if (t - g.lastTickT >= tickInterval) {
      g.lastTickT = t;

      if (autoMode) {
        g.pendingDir = autoChoose(nearMiss);
        g.hasPendingDir = true;
        s_inputCount = 0;  // manual inputs ignored while auto is on
      }

      if (g.hasPendingDir) {
        g.direction = g.pendingDir;
        g.hasPendingDir = false;
      }

      const int* step = STEP[(int)g.direction];
      const Segment head = g.segments[0];
      const int nx = head.x + step[0], ny = head.y + step[1], nz = head.z + step[2];

      if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) {
        g.alive = false;
        g.deathStartT = t;
      } else {
        const bool ate = nx == g.apple.x && ny == g.apple.y && nz == g.apple.z;
        // The tail tip is about to move (unless eating), so don't check it.
        const int checkLen = ate ? g.length : g.length - 1;
        bool selfHit = false;
        for (int i = 0; i < checkLen; i++) {
          if (g.segments[i].x == nx && g.segments[i].y == ny && g.segments[i].z == nz) {
            selfHit = true;
            break;
          }
        }
        if (selfHit) {
          g.alive = false;
          g.deathStartT = t;
        } else {
          // unshift new head; pop tail unless eating.
          const int newLen = ate ? g.length + 1 : g.length;
          std::memmove(g.segments + 1, g.segments,
                       (newLen - 1) * sizeof(Segment));
          g.segments[0] = {(int8_t)nx, (int8_t)ny, (int8_t)nz};
          g.length = newLen;
          if (ate) {
            g.score++;
            if (g.score > g.highScore) g.highScore = g.score;
            g.apple = randomEmpty();
          }
        }
      }
    }
  } else if (!g.alive) {
    if (t - g.deathStartT > 1.5f) reset(g.highScore);
  }
  // (alive but !started: snake idles; tick logic skipped)

  std::memset(buffer, 0, NUM_LEDS * 3);

  if (!g.alive) {
    // Death flash: red strobe along the snake, fading out.
    const float elapsed = t - g.deathStartT;
    const float fade = std::fmax(0.0f, 1.0f - elapsed / 1.5f);
    const float strobe = std::fabs(std::sin(elapsed * 14.0f));
    const uint8_t r = (uint8_t)std::lround(255.0f * strobe * fade);
    for (int i = 0; i < g.length; i++) {
      buffer[ctx.idx(g.segments[i].x, g.segments[i].y, g.segments[i].z) * 3] = r;
    }
  } else {
    for (int s = 0; s < g.length; s++) {
      const Segment& seg = g.segments[s];
      const float t01 = 1.0f - (float)s / std::fmax(1, g.length);
      uint8_t rgb[3];
      samplePalette(pal, t01, t, rgb);
      const float fall = 0.35f + 0.65f * t01;
      const int i = ctx.idx(seg.x, seg.y, seg.z) * 3;
      buffer[i] = (uint8_t)std::lround(rgb[0] * fall);
      buffer[i + 1] = (uint8_t)std::lround(rgb[1] * fall);
      buffer[i + 2] = (uint8_t)std::lround(rgb[2] * fall);
    }
    // Apple: gentle pulse.
    const float pulse = 0.65f + 0.35f * std::sin(t * 6.0f);
    const int ai = ctx.idx(g.apple.x, g.apple.y, g.apple.z) * 3;
    buffer[ai] = (uint8_t)std::lround(appleR * pulse);
    buffer[ai + 1] = (uint8_t)std::lround(appleG * pulse);
    buffer[ai + 2] = (uint8_t)std::lround(appleB * pulse);

    // Training hint overlay: fades by time-since-press and by score.
    if (g.hasHint) {
      const float scoreF =
          g.score < hintFullScore
              ? 1.0f
              : std::fmax(0.0f, 1.0f - (g.score - hintFullScore) /
                                           (hintGoneScore - hintFullScore));
      const float flashF = std::fmax(0.0f, 1.0f - (t - g.hintStartT) / hintFlashSec);
      const float intensity = scoreF * flashF;
      if (intensity > 0.02f) drawDirectionHint(ctx, g.hintDir, g.segments[0], intensity);
    }
  }
}

}  // namespace

void queueSnakeInput(SnakeDir dir) {
  if (!s_gameInit || !s_game.alive) return;
  // Drop reverse-of-current so a queued input can never insta-kill.
  if (dir == opposite(s_game.direction)) return;
  if (!s_game.started) {
    // First input both starts movement and sets the initial direction.
    s_game.started = true;
    s_game.direction = dir;
  } else if (s_inputCount < kInputQueueMax) {
    s_inputQueue[s_inputCount++] = dir;
  }
  s_pendingHint = dir;
  s_hasPendingHint = true;
}

bool getSnakeState(SnakeState& out) {
  if (!s_gameInit) return false;
  out = {s_game.score, s_game.highScore, s_game.alive, s_game.length};
  return true;
}

extern const Pattern kSnake3d = {"snake-3d", init, render};

}  // namespace cube
