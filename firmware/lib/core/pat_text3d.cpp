// Port of src/shared/patterns/text3d.ts — 10x10 glyph marquee with three
// modes (planes / ring / stack). Glyph data comes from cube_font10.h,
// generated from src/shared/fonts/builtin10.ts.
#include <cmath>
#include <cstring>

#include "cube_color.h"
#include "cube_font10.h"
#include "cube_palettes.h"
#include "cube_pattern.h"
#include "cube_throb.h"

namespace cube {
namespace {

// Integrated each frame so rate can vary with audio without phase jumps.
float s_lastT = 0;
float s_scrollPos = 0;  // voxels, 'planes' mode
float s_charPos = 0;    // chars, 'ring'/'stack' modes
AudioThrob s_throb;

void init(PatternCtx& ctx) {
  s_lastT = ctx.t;
  s_scrollPos = 0;
  s_charPos = 0;
  s_throb.reset();
  std::memset(ctx.buffer, 0, NUM_LEDS * 3);
}

struct Ctx {
  PatternCtx* pc;
  char axis;
  bool useP;
  PaletteRef pal;
  float cr, cg, cb;
};

inline int setIdx(const Ctx& c, int a, int b, int h) {
  if (c.axis == 'z') return c.pc->idx(a, b, h);
  if (c.axis == 'y') return c.pc->idx(a, h, b);
  return c.pc->idx(h, a, b);
}

inline void writePixel(uint8_t* buffer, int i, float weight, float pr, float pg, float pb) {
  if (weight <= 0) return;
  const int off = i * 3;
  const uint8_t rr = (uint8_t)std::lround(pr * weight);
  const uint8_t gg = (uint8_t)std::lround(pg * weight);
  const uint8_t bb = (uint8_t)std::lround(pb * weight);
  if (rr > buffer[off]) buffer[off] = rr;
  if (gg > buffer[off + 1]) buffer[off + 1] = gg;
  if (bb > buffer[off + 2]) buffer[off + 2] = bb;
}

inline void glyphColor(const Ctx& c, int gx, int gy, float out[3]) {
  if (!c.useP) {
    out[0] = c.cr;
    out[1] = c.cg;
    out[2] = c.cb;
    return;
  }
  // Sample palette across the glyph diagonal for a subtle per-char gradient.
  const float t01 = (float)(gx + gy) / (FONT_W + FONT_H - 2);
  uint8_t rgb[3];
  samplePalette(c.pal, clamp01(t01), c.pc->t, rgb);
  out[0] = rgb[0];
  out[1] = rgb[1];
  out[2] = rgb[2];
}

void render(PatternCtx& ctx) {
  const Params& p = *ctx.params;
  const AudioFrame& audio = *ctx.audio;
  uint8_t* buffer = ctx.buffer;
  const float t = ctx.t;
  const float dt = std::fmax(0.0f, std::fmin(0.1f, t - s_lastT));
  s_lastT = t;
  std::memset(buffer, 0, NUM_LEDS * 3);

  const int N = CUBE_N;
  const char mode = p.str("mode", "planes")[0];  // 'p' | 'r' | 's'
  const char* text = p.str("text", "HELLO 123 ");
  const float baseSpeed = p.num("speed", 3.0f);
  const bool reverse = p.boolean("reverse", false);
  const int charSpacing = (int)std::fmax(1.0f, std::floor(p.num("charSpacing", 4.0f)));
  const float levelGain = p.num("levelGain", 2.0f);
  const float beatGain = p.num("beatGain", 1.5f);
  const bool highlightLead = p.boolean("highlightLead", false);
  const float trailDim = clamp01(p.num("trailDim", 0.8f));
  const bool leadAtFront = p.str("leadAt", "front")[0] == 'f';
  const int clipL = (int)std::fmax(0.0f, std::fmin(4.0f, std::floor(p.num("clipL", 0.0f))));
  const int clipR = (int)std::fmax(0.0f, std::fmin(4.0f, std::floor(p.num("clipR", 1.0f))));

  Ctx c;
  c.pc = &ctx;
  c.axis = p.str("axis", "z")[0];
  c.cr = p.num("r", 255.0f);
  c.cg = p.num("g", 240.0f);
  c.cb = p.num("b", 200.0f);
  const char* paletteName = p.str("palette", "none");
  c.useP = paletteActive(paletteName);
  c.pal = resolvePalette(paletteName);

  const float level = clamp01(audio.level);
  const float beat = audio.beat;
  const float audioMult = 1.0f + level * levelGain + beat * beatGain;
  // Beat-synced brightness throb (shared module; same knobs as snake-3d).
  const float throb = s_throb.update(ctx);

  int charCount = (int)std::strlen(text);
  const char* chars = charCount > 0 ? text : " ";
  if (charCount == 0) charCount = 1;

  if (mode == 'p') {
    // planes: characters stacked along the scroll axis, moving over time.
    s_scrollPos += baseSpeed * audioMult * dt * (reverse ? -1.0f : 1.0f);
    const float totalLen = (float)charCount * charSpacing;
    if (totalLen > 0) {
      s_scrollPos = std::fmod(std::fmod(s_scrollPos, totalLen) + totalLen, totalLen);
    }

    const int iMin = (int)std::floor((s_scrollPos - 1) / charSpacing);
    const int iMax = (int)std::ceil((s_scrollPos + N) / charSpacing);

    // Which char is the "lead" (front of motion) for highlight mode.
    int leadI = INT32_MIN;
    if (highlightLead) {
      const bool wantSmallest = leadAtFront != reverse;
      float bestPos = wantSmallest ? 1e9f : -1e9f;
      for (int i = iMin; i <= iMax; i++) {
        const float pos = i * charSpacing - s_scrollPos;
        if (pos < -1 || pos > N) continue;
        if ((wantSmallest && pos < bestPos) || (!wantSmallest && pos > bestPos)) {
          bestPos = pos;
          leadI = i;
        }
      }
    }

    for (int i = iMin; i <= iMax; i++) {
      const float pos = i * charSpacing - s_scrollPos;
      if (pos < -1 || pos > N) continue;
      int ci = ((i % charCount) + charCount) % charCount;
      // Reverse flips only the direction of travel; mirror the char-to-slot
      // mapping too so the first letter still leads the motion — without this
      // the message passes the viewer in back-to-front letter order.
      if (reverse) ci = (charCount - ci) % charCount;
      const char ch = chars[ci];
      const uint16_t* grid = getGlyph10(ch);
      const float charBright = (highlightLead && i != leadI ? trailDim : 1.0f) * throb;

      // Antialias across two adjacent slices using the fractional part.
      const int h0 = (int)std::floor(pos);
      const float fr = pos - h0;
      for (int gy = 0; gy < FONT_H; gy++) {
        const uint16_t row = grid[gy];
        for (int gx = 0; gx < FONT_W; gx++) {
          if (!((row >> gx) & 1)) continue;
          // Glyph row 0 = top -> cube b = N-1-gy.
          const int a = gx;
          const int b = N - 1 - gy;
          float col[3];
          glyphColor(c, gx, gy, col);
          if (h0 >= 0 && h0 < N)
            writePixel(buffer, setIdx(c, a, b, h0), (1 - fr) * charBright, col[0], col[1], col[2]);
          if (h0 + 1 >= 0 && h0 + 1 < N)
            writePixel(buffer, setIdx(c, a, b, h0 + 1), fr * charBright, col[0], col[1], col[2]);
        }
      }
    }
  } else if (mode == 's') {
    // stack: same character on every slice, palette gradient across slices.
    s_charPos += baseSpeed * audioMult * 0.5f * dt;
    s_charPos = std::fmod(std::fmod(s_charPos, (float)charCount) + charCount, (float)charCount);
    const int i0 = (int)s_charPos;
    const float fr = s_charPos - i0;
    const float fadeIn = fr < 0.85f ? 0.0f : (fr - 0.85f) / 0.15f;

    auto drawStack = [&](char ch, float weight) {
      weight *= throb;
      if (weight <= 0) return;
      const uint16_t* grid = getGlyph10(ch);
      for (int h = 0; h < N; h++) {
        const float planeT = reverse ? 1.0f - (float)h / (N - 1) : (float)h / (N - 1);
        float col[3] = {c.cr, c.cg, c.cb};
        if (c.useP) {
          uint8_t rgb[3];
          samplePalette(c.pal, planeT, t, rgb);
          col[0] = rgb[0];
          col[1] = rgb[1];
          col[2] = rgb[2];
        }
        for (int gy = 0; gy < FONT_H; gy++) {
          const uint16_t row = grid[gy];
          for (int gx = 0; gx < FONT_W; gx++) {
            if (!((row >> gx) & 1)) continue;
            writePixel(buffer, setIdx(c, gx, N - 1 - gy, h), weight, col[0], col[1], col[2]);
          }
        }
      }
    };
    // Between words (space chars) the stack normally goes fully dark for a
    // whole dwell. stackGap softens that: "dot" swaps in a period, "dim"
    // previews the next real character at low brightness.
    const char* gapStyle = p.str("stackGap", "blank");
    auto drawSlot = [&](int slot, float weight) {
      char ch = chars[slot % charCount];
      if (ch == ' ' && gapStyle[0] == 'd') {
        if (gapStyle[1] == 'o') {
          ch = '.';
        } else {
          for (int k = 1; k <= charCount; k++) {
            const char nc = chars[(slot + k) % charCount];
            if (nc != ' ') {
              ch = nc;
              break;
            }
          }
          weight *= 0.3f;
        }
      }
      drawStack(ch, weight);
    };
    drawSlot(i0, 1.0f - fadeIn);
    if (fadeIn > 0) drawSlot(i0 + 1, fadeIn);
  } else {
    // ring: current char on all 4 vertical faces, cycling through the text.
    s_charPos += baseSpeed * audioMult * 0.5f * dt;
    s_charPos = std::fmod(std::fmod(s_charPos, (float)charCount) + charCount, (float)charCount);
    const int i0 = (int)s_charPos;
    const float fr = s_charPos - i0;
    // Crossfade only in the last 15% of the dwell so chars stay readable.
    const float fadeIn = fr < 0.85f ? 0.0f : (fr - 0.85f) / 0.15f;

    auto drawChar = [&](char ch, float weight) {
      weight *= throb;
      if (weight <= 0) return;
      const uint16_t* grid = getGlyph10(ch);
      for (int face = 0; face < 4; face++) {
        for (int gy = 0; gy < FONT_H; gy++) {
          const uint16_t row = grid[gy];
          for (int gx = clipL; gx < FONT_W - clipR; gx++) {
            if (!((row >> gx) & 1)) continue;
            const int v = N - 1 - gy;  // glyph top -> high end of vertical axis
            const int h = gx;
            int cx = 0, cy = 0, cz = 0;
            if (c.axis == 'z') {
              cz = v;
              if (face == 0) { cx = h; cy = 0; }
              else if (face == 1) { cx = N - 1; cy = h; }
              else if (face == 2) { cx = N - 1 - h; cy = N - 1; }
              else { cx = 0; cy = N - 1 - h; }
            } else if (c.axis == 'y') {
              cy = v;
              if (face == 0) { cx = h; cz = 0; }
              else if (face == 1) { cx = N - 1; cz = h; }
              else if (face == 2) { cx = N - 1 - h; cz = N - 1; }
              else { cx = 0; cz = N - 1 - h; }
            } else {
              cx = v;
              if (face == 0) { cy = h; cz = 0; }
              else if (face == 1) { cy = N - 1; cz = h; }
              else if (face == 2) { cy = N - 1 - h; cz = N - 1; }
              else { cy = 0; cz = N - 1 - h; }
            }
            float col[3];
            glyphColor(c, gx, gy, col);
            writePixel(buffer, ctx.idx(cx, cy, cz), weight, col[0], col[1], col[2]);
          }
        }
      }
    };
    drawChar(chars[i0 % charCount], 1.0f - fadeIn);
    if (fadeIn > 0) drawChar(chars[(i0 + 1) % charCount], fadeIn);
  }
}

}  // namespace

extern const Pattern kText3d = {"text-3d", init, render};

}  // namespace cube
