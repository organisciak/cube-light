// Port of src/shared/patterns/pacman3d.ts — pellets, chomping head, rainbow
// trail, greedy ghosts, BFS auto-solver with ghost halos.
#include <algorithm>
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_pacman.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_random.h"

namespace cube {
namespace {

constexpr int N = CUBE_N;
constexpr int N3 = NUM_LEDS;
constexpr float kPi = 3.14159265358979f;

const int STEP[6][3] = {
    {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
};
inline int opposite(int d) { return d ^ 1; }

struct Cell {
  int8_t x, y, z;
};

const uint8_t GHOST_COLORS[4][3] = {
    {255, 0, 0},      // Blinky
    {255, 184, 255},  // Pinky
    {0, 255, 255},    // Inky
    {255, 184, 82},   // Clyde
};

struct Ghost {
  Cell pos;
  const uint8_t* color;
  int lastDir;  // -1 = none
};

constexpr int kMaxTail = 64;

struct PacGame {
  Cell pac;
  int dir;
  int pendingDir;  // -1 = none
  uint8_t pellets[N3];
  int pelletsLeft;
  Cell trail[kMaxTail];  // head-first
  int trailLen;
  Ghost ghosts[4];
  int ghostCount;
  bool alive;
  bool won;
  float lastTickT;
  float lastGhostT;
  float deathStartT;
  float winStartT;
  int score;
  float beatBoost;
  float chompPhase;
};

PacGame s_game;
bool s_gameInit = false;

constexpr int kInputQueueMax = 8;
int s_inputQueue[kInputQueueMax];
int s_inputCount = 0;

inline int cellIdx(int x, int y, int z) { return x + y * N + z * N * N; }
inline bool inBounds(int x, int y, int z) {
  return x >= 0 && x < N && y >= 0 && y < N && z >= 0 && z < N;
}

void spawnGhost(int i) {
  const Cell corners[4] = {
      {0, 0, 0}, {N - 1, 0, 0}, {0, N - 1, 0}, {N - 1, N - 1, 0}};
  s_game.ghosts[i] = {corners[i % 4], GHOST_COLORS[i % 4], -1};
}

void reset() {
  std::memset(s_game.pellets, 1, sizeof(s_game.pellets));
  const int8_t c = N / 2;
  s_game.pellets[cellIdx(c, c, c)] = 0;  // spawn cell counts as eaten
  s_game.ghostCount = 4;
  for (int i = 0; i < 4; i++) {
    spawnGhost(i);
    s_game.pellets[cellIdx(s_game.ghosts[i].pos.x, s_game.ghosts[i].pos.y,
                           s_game.ghosts[i].pos.z)] = 0;
  }
  s_game.pac = {c, c, c};
  s_game.dir = 0;  // x+
  s_game.pendingDir = -1;
  s_game.pelletsLeft = N3 - 1 - 4;
  s_game.trailLen = 0;
  s_game.alive = true;
  s_game.won = false;
  s_game.lastTickT = 0;
  s_game.lastGhostT = 0;
  s_game.deathStartT = 0;
  s_game.winStartT = 0;
  s_game.score = 0;
  s_game.beatBoost = 0;
  s_game.chompPhase = 0;
  s_inputCount = 0;
  s_gameInit = true;
}

// Soft low-saturation pellet color from a hue (port of hueToRgb, s=0.7).
void pelletHueToRgb(float h, uint8_t out[3]) {
  const float hh = h - std::floor(h);
  const int i = (int)(hh * 6);
  const float f = hh * 6 - i;
  const float s = 0.7f, v = 1.0f;
  const float p = v * (1 - s);
  const float q = v * (1 - f * s);
  const float w = v * (1 - (1 - f) * s);
  float r = 0, g = 0, b = 0;
  switch (i % 6) {
    case 0: r = v; g = w; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = w; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = w; g = p; b = v; break;
    case 5: r = v; g = p; b = q; break;
  }
  out[0] = (uint8_t)std::lround(r * 255);
  out[1] = (uint8_t)std::lround(g * 255);
  out[2] = (uint8_t)std::lround(b * 255);
}

// BFS toward the nearest pellet, ghost cells + 1-cell halo blocked; records
// the first-step direction per visited cell. Fallback: maximize distance
// from the nearest ghost.
int autoChooseForPac() {
  const PacGame& g = s_game;
  const Cell start = g.pac;
  static uint8_t blocked[N3];
  static int16_t dist[N3];
  static int8_t firstDir[N3];
  static uint16_t queue[N3];

  std::memset(blocked, 0, sizeof(blocked));
  for (int gi = 0; gi < g.ghostCount; gi++) {
    const Ghost& gh = g.ghosts[gi];
    blocked[cellIdx(gh.pos.x, gh.pos.y, gh.pos.z)] = 1;
    for (int d = 0; d < 6; d++) {
      const int hx = gh.pos.x + STEP[d][0];
      const int hy = gh.pos.y + STEP[d][1];
      const int hz = gh.pos.z + STEP[d][2];
      if (inBounds(hx, hy, hz)) blocked[cellIdx(hx, hy, hz)] = 1;
    }
  }
  blocked[cellIdx(start.x, start.y, start.z)] = 0;

  std::memset(dist, -1, sizeof(dist));
  std::memset(firstDir, -1, sizeof(firstDir));
  const int sIdx = cellIdx(start.x, start.y, start.z);
  dist[sIdx] = 0;
  int qHead = 0, qTail = 0;
  queue[qTail++] = (uint16_t)sIdx;
  int bestPellet = -1;
  while (qHead < qTail) {
    const int cur = queue[qHead++];
    if (g.pellets[cur]) {
      bestPellet = cur;
      break;
    }
    const int cx = cur % N;
    const int cy = (cur / N) % N;
    const int cz = cur / (N * N);
    for (int d = 0; d < 6; d++) {
      // Don't reverse on the first move — looks indecisive.
      if (cur == sIdx && d == opposite(g.dir)) continue;
      const int nx = cx + STEP[d][0], ny = cy + STEP[d][1], nz = cz + STEP[d][2];
      if (!inBounds(nx, ny, nz)) continue;
      const int ni = cellIdx(nx, ny, nz);
      if (dist[ni] != -1 || blocked[ni]) continue;
      dist[ni] = (int16_t)(dist[cur] + 1);
      firstDir[ni] = cur == sIdx ? (int8_t)d : firstDir[cur];
      queue[qTail++] = (uint16_t)ni;
    }
  }

  if (bestPellet >= 0 && firstDir[bestPellet] >= 0) return firstDir[bestPellet];

  // Fallback: whichever step maximizes min-distance to the ghosts.
  int bestDir = g.dir;
  int bestScore = -1;
  for (int d = 0; d < 6; d++) {
    if (d == opposite(g.dir)) continue;
    const int nx = start.x + STEP[d][0], ny = start.y + STEP[d][1], nz = start.z + STEP[d][2];
    if (!inBounds(nx, ny, nz)) continue;
    if (blocked[cellIdx(nx, ny, nz)]) continue;
    int minGhost = 0x7fff;
    for (int gi = 0; gi < g.ghostCount; gi++) {
      const Ghost& gh = g.ghosts[gi];
      const int dd = std::abs(gh.pos.x - nx) + std::abs(gh.pos.y - ny) + std::abs(gh.pos.z - nz);
      minGhost = std::min(minGhost, dd);
    }
    if (minGhost > bestScore) {
      bestScore = minGhost;
      bestDir = d;
    }
  }
  return bestDir;
}

// Greedy chase with 15% wander; ghosts avoid stacking and avoid reversing
// unless stuck.
void stepGhosts() {
  PacGame& g = s_game;
  const Cell target = g.pac;
  for (int gi = 0; gi < g.ghostCount; gi++) {
    Ghost& gh = g.ghosts[gi];
    struct Candidate {
      int dir;
      Cell cell;
      int dist;
    };
    Candidate candidates[6];
    int candCount = 0;

    auto tryDir = [&](int d, bool allowReverse) {
      if (!allowReverse && gh.lastDir >= 0 && d == opposite(gh.lastDir)) return;
      const int nx = gh.pos.x + STEP[d][0];
      const int ny = gh.pos.y + STEP[d][1];
      const int nz = gh.pos.z + STEP[d][2];
      if (!inBounds(nx, ny, nz)) return;
      if (!allowReverse) {
        for (int oi = 0; oi < g.ghostCount; oi++) {
          if (oi != gi && g.ghosts[oi].pos.x == nx && g.ghosts[oi].pos.y == ny &&
              g.ghosts[oi].pos.z == nz)
            return;
        }
      }
      const int dd = std::abs(target.x - nx) + std::abs(target.y - ny) + std::abs(target.z - nz);
      candidates[candCount++] = {d, {(int8_t)nx, (int8_t)ny, (int8_t)nz}, dd};
    };

    for (int d = 0; d < 6; d++) tryDir(d, false);
    if (candCount == 0) {
      for (int d = 0; d < 6; d++) tryDir(d, true);
      if (candCount == 0) continue;
    }
    std::sort(candidates, candidates + candCount,
              [](const Candidate& a, const Candidate& b) { return a.dist < b.dist; });
    const Candidate& chosen =
        frand() < 0.15f ? candidates[(int)(frand() * candCount)] : candidates[0];
    gh.pos = chosen.cell;
    gh.lastDir = chosen.dir;
  }
}

void init(PatternCtx& ctx) {
  reset();
  s_game.lastTickT = ctx.t;
  s_game.lastGhostT = ctx.t;
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

void render(PatternCtx& ctx) {
  if (!s_gameInit) reset();
  PacGame& g = s_game;
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = ctx.dt;

  const bool autoMode = p.str("mode", "auto")[0] == 'a';
  const float baseSpeed = p.num("baseSpeed", 6.0f);
  const float ghostSpeed = p.num("ghostSpeed", 3.0f);
  const int ghostCount = (int)std::fmax(0.0f, std::fmin(4.0f, std::floor(p.num("ghostCount", 4.0f))));
  const int tailLen = std::min(kMaxTail, (int)std::fmax(0.0f, std::floor(p.num("tailLength", 14.0f))));
  const PaletteRef pal = resolvePalette(p.str("palette", "rainbow"), p, ctx.t);
  const float pelletB = clamp01(p.num("pelletBrightness", 0.06f));
  const float pelletHue = clamp01(p.num("pelletColor", 0.13f));
  const float beatSpeedGain = p.num("beatSpeedGain", 1.5f);
  const float beatSparkle = p.num("beatSparkle", 0.6f);
  const float levelGain = p.num("levelGain", 0.5f);
  const float wakaHz = p.num("wakaHz", 5.0f);
  const bool ghostFearOnBeat = p.boolean("ghostFearOnBeat", true);

  const float beat = audio.beat;
  const float level = audio.level;
  // Beat boost decays exponentially; new beats clamp to whichever is larger.
  g.beatBoost = std::fmax(g.beatBoost * std::exp(-dt * 2.5f), beat * beatSpeedGain);
  const float speedMul = 1.0f + g.beatBoost;

  // Ghost count can change live.
  if (g.ghostCount > ghostCount) g.ghostCount = ghostCount;
  while (g.ghostCount < ghostCount) spawnGhost(g.ghostCount++);

  // --- Pac-Man tick ---
  if (g.alive && !g.won) {
    const float tickInterval = 1.0f / std::fmax(0.5f, baseSpeed * speedMul);
    if (t - g.lastTickT >= tickInterval) {
      g.lastTickT = t;

      if (autoMode) {
        g.pendingDir = autoChooseForPac();
        s_inputCount = 0;
      } else if (g.pendingDir < 0 && s_inputCount > 0) {
        const int next = s_inputQueue[0];
        s_inputCount--;
        std::memmove(s_inputQueue, s_inputQueue + 1, s_inputCount * sizeof(int));
        if (next != opposite(g.dir)) g.pendingDir = next;
      }
      if (g.pendingDir >= 0) {
        g.dir = g.pendingDir;
        g.pendingDir = -1;
      }

      int nx = g.pac.x + STEP[g.dir][0];
      int ny = g.pac.y + STEP[g.dir][1];
      int nz = g.pac.z + STEP[g.dir][2];

      // Wall reflection: Pac-Man bounces rather than dying on walls.
      if (!inBounds(nx, ny, nz)) {
        g.dir = opposite(g.dir);
        nx = g.pac.x + STEP[g.dir][0];
        ny = g.pac.y + STEP[g.dir][1];
        nz = g.pac.z + STEP[g.dir][2];
      }

      // Apply move; trail records the previous cell.
      const Cell prev = g.pac;
      g.pac = {(int8_t)nx, (int8_t)ny, (int8_t)nz};
      if (tailLen > 0) {
        const int newLen = std::min(tailLen, g.trailLen + 1);
        std::memmove(g.trail + 1, g.trail, (newLen - 1) * sizeof(Cell));
        g.trail[0] = prev;
        g.trailLen = newLen;
      } else {
        g.trailLen = 0;
      }

      const int cIdx = cellIdx(nx, ny, nz);
      if (g.pellets[cIdx]) {
        g.pellets[cIdx] = 0;
        g.pelletsLeft--;
        g.score++;
      }

      if (g.pelletsLeft <= 0) {
        g.won = true;
        g.winStartT = t;
      }

      for (int gi = 0; gi < g.ghostCount; gi++) {
        const Ghost& gh = g.ghosts[gi];
        if (gh.pos.x == nx && gh.pos.y == ny && gh.pos.z == nz) {
          g.alive = false;
          g.deathStartT = t;
          break;
        }
      }
    }
  }

  // --- Ghost tick (independent rate) ---
  if (g.alive && !g.won) {
    const float ghostInterval = 1.0f / std::fmax(0.25f, ghostSpeed * (1.0f + g.beatBoost * 0.4f));
    if (t - g.lastGhostT >= ghostInterval) {
      g.lastGhostT = t;
      stepGhosts();
      for (int gi = 0; gi < g.ghostCount; gi++) {
        const Ghost& gh = g.ghosts[gi];
        if (gh.pos.x == g.pac.x && gh.pos.y == g.pac.y && gh.pos.z == g.pac.z) {
          g.alive = false;
          g.deathStartT = t;
          break;
        }
      }
    }
  }

  if (g.won && t - g.winStartT > 2.0f) reset();
  if (!g.alive && t - g.deathStartT > 1.5f) reset();

  // ----- render -----
  std::memset(buffer, 0, NUM_LEDS * 3);

  // Pellet field: dim baseline + audio sparkle + per-cell twinkle.
  uint8_t pelletColor[3];
  pelletHueToRgb(pelletHue, pelletColor);
  const float ambient = pelletB * (1.0f + level * levelGain) + beat * beatSparkle * 0.3f;
  if (ambient > 0.001f) {
    for (int z = 0; z < N; z++) {
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          if (!g.pellets[cellIdx(x, y, z)]) continue;
          const float twink = 0.85f + 0.15f * std::sin(t * 4 + x * 1.7f + y * 2.3f + z * 0.9f);
          const float k = clamp01(ambient * twink);
          const int i = ctx.idx(x, y, z) * 3;
          buffer[i] = (uint8_t)std::lround(pelletColor[0] * k);
          buffer[i + 1] = (uint8_t)std::lround(pelletColor[1] * k);
          buffer[i + 2] = (uint8_t)std::lround(pelletColor[2] * k);
        }
      }
    }
  }

  if (g.won) {
    // Rainbow celebration.
    const float fade = clamp01(1.0f - (t - g.winStartT) / 2.0f);
    for (int z = 0; z < N; z++) {
      for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
          const float h = (float)(x + y + z) / (3 * (N - 1)) + t * 0.6f;
          uint8_t rgb[3];
          samplePalette(pal, h - std::floor(h), t, rgb);
          const float k = fade * (0.6f + 0.4f * std::sin(t * 8 + x + y + z));
          const int i = ctx.idx(x, y, z) * 3;
          buffer[i] = (uint8_t)std::lround(rgb[0] * k);
          buffer[i + 1] = (uint8_t)std::lround(rgb[1] * k);
          buffer[i + 2] = (uint8_t)std::lround(rgb[2] * k);
        }
      }
    }
    return;
  }

  // Tail (rainbow), before the head so the head wins on overlap.
  for (int s = 0; s < g.trailLen; s++) {
    const Cell& seg = g.trail[s];
    const float t01 = 1.0f - (float)s / std::fmax(1, g.trailLen);
    const float hp = t01 + t * 0.3f;
    uint8_t rgb[3];
    samplePalette(pal, hp - std::floor(hp), t, rgb);
    const float fall = 0.25f + 0.75f * t01;
    const int i = ctx.idx(seg.x, seg.y, seg.z) * 3;
    buffer[i] = (uint8_t)std::lround(rgb[0] * fall);
    buffer[i + 1] = (uint8_t)std::lround(rgb[1] * fall);
    buffer[i + 2] = (uint8_t)std::lround(rgb[2] * fall);
  }

  if (!g.alive) {
    // Death flash: red strobe at Pac-Man's last position.
    const float elapsed = t - g.deathStartT;
    const float fade = clamp01(1.0f - elapsed / 1.5f);
    const float strobe = std::fabs(std::sin(elapsed * 18.0f));
    buffer[ctx.idx(g.pac.x, g.pac.y, g.pac.z) * 3] =
        (uint8_t)std::lround(255.0f * strobe * fade);
  } else {
    // Chomping yellow head with audio brightness kick.
    g.chompPhase = std::fmod(g.chompPhase + dt * wakaHz, 1.0f);
    const float chomp = 0.5f + 0.5f * std::cos(g.chompPhase * kPi * 2.0f);
    const float yk = 0.55f + 0.45f * chomp + std::fmin(0.4f, level * 0.6f) +
                     std::fmin(0.3f, beat * 0.5f);
    const float yScale = clamp01(yk);
    const int i = ctx.idx(g.pac.x, g.pac.y, g.pac.z) * 3;
    buffer[i] = (uint8_t)std::lround(255.0f * yScale);
    buffer[i + 1] = (uint8_t)std::lround((180.0f + 75.0f * chomp) * yScale);
    buffer[i + 2] = 0;
  }

  // Ghosts on top; beat tints them the classic "scared" blue.
  const float fearFlick = ghostFearOnBeat ? clamp01(beat) : 0.0f;
  for (int gi = 0; gi < g.ghostCount; gi++) {
    const Ghost& gh = g.ghosts[gi];
    const int i = ctx.idx(gh.pos.x, gh.pos.y, gh.pos.z) * 3;
    buffer[i] = (uint8_t)std::lround(gh.color[0] * (1 - fearFlick) + 60 * fearFlick);
    buffer[i + 1] = (uint8_t)std::lround(gh.color[1] * (1 - fearFlick) + 60 * fearFlick);
    buffer[i + 2] = (uint8_t)std::lround(gh.color[2] * (1 - fearFlick) + 255 * fearFlick);
  }
}

}  // namespace

void queuePacmanInput(SnakeDir dir) {
  if (!s_gameInit || !s_game.alive) return;
  if ((int)dir == opposite(s_game.dir)) return;
  if (s_inputCount < kInputQueueMax) s_inputQueue[s_inputCount++] = (int)dir;
}

extern const Pattern kPacman3d = {"pacman-3d", init, render};

}  // namespace cube
