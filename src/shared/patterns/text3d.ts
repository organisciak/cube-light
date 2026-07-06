import { CUBE_N } from '../types';
import { clamp01, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import { FONT_H, FONT_W, getGlyph } from '../fonts/builtin10';
import type { Pattern } from './types';

// Module state. `scrollPos` is in voxels for 'planes' mode; `charPos` is in
// chars for 'ring' mode. Both are integrated each frame so the rate can vary
// with audio without phase jumps.
let lastT = 0;
let scrollPos = 0;
let charPos = 0;

/**
 * 10×10 text marquee.
 *
 * Two modes:
 * - **planes**: Each character is one 10×10 sheet on a chosen plane; sheets
 *   are stacked along the perpendicular scroll axis with `charSpacing` voxels
 *   between them, and the whole stack moves forward over time. Audio level +
 *   beat speed it up.
 * - **ring**: A single character is rendered on all 4 vertical faces of the
 *   cube simultaneously. Cycles through the text at an audio-modulated rate.
 *   `clipL`/`clipR` trim columns on each face so adjacent faces don't double
 *   up on the shared corner column (each face owns 9 of its 10 cols).
 */
export const text3d: Pattern = {
  meta: {
    id: 'text-3d',
    name: 'Text Marquee',
    description: '10×10 glyphs scroll through the cube; alt mode wraps a single char around the outside.',
    params: [
      { key: 'mode', label: 'Mode', type: 'enum', options: ['planes', 'ring', 'stack'], default: 'planes' },
      { key: 'text', label: 'Text', type: 'string', default: 'HELLO 123 ' },
      { key: 'axis', label: 'Axis', type: 'enum', options: ['x', 'y', 'z'], default: 'z' },
      { key: 'reverse', label: 'Reverse axis direction', type: 'bool', default: false },
      { key: 'speed', label: 'Base speed', type: 'number', min: 0, max: 20, step: 0.1, default: 3 },
      { key: 'charSpacing', label: 'Char spacing (voxels)', type: 'number', min: 1, max: 30, step: 1, default: 4 },
      { key: 'levelGain', label: 'Audio level → speed', type: 'number', min: 0, max: 6, step: 0.1, default: 2 },
      { key: 'beatGain', label: 'Beat → speed kick', type: 'number', min: 0, max: 6, step: 0.1, default: 1.5 },
      { key: 'highlightLead', label: 'Planes: highlight lead', type: 'bool', default: false },
      { key: 'trailDim', label: 'Planes: trail brightness', type: 'number', min: 0, max: 1, step: 0.05, default: 0.8 },
      { key: 'leadAt', label: 'Planes: lead at', type: 'enum', options: ['front', 'back'], default: 'front' },
      { key: 'clipL', label: 'Ring: clip left cols', type: 'number', min: 0, max: 4, step: 1, default: 0 },
      { key: 'clipR', label: 'Ring: clip right cols', type: 'number', min: 0, max: 4, step: 1, default: 1 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'none' },
      { key: 'r', label: 'R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'g', label: 'G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 240 },
      { key: 'b', label: 'B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 200 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    scrollPos = 0;
    charPos = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const N = CUBE_N;
    const mode = String(params.mode ?? 'planes') as 'planes' | 'ring' | 'stack';
    const text = String(params.text ?? 'HELLO ');
    const axis = String(params.axis ?? 'z') as 'x' | 'y' | 'z';
    const baseSpeed = num(params.speed, 3);
    const reverse = Boolean(params.reverse ?? false);
    const charSpacing = Math.max(1, Math.floor(num(params.charSpacing, 4)));
    const levelGain = num(params.levelGain, 2);
    const beatGain = num(params.beatGain, 1.5);
    const highlightLead = Boolean(params.highlightLead ?? false);
    const trailDim = clamp01(num(params.trailDim, 0.8));
    const leadAt = String(params.leadAt ?? 'front') as 'front' | 'back';
    const clipL = Math.max(0, Math.min(4, Math.floor(num(params.clipL, 0))));
    const clipR = Math.max(0, Math.min(4, Math.floor(num(params.clipR, 1))));
    const cr = num(params.r, 255);
    const cg = num(params.g, 240);
    const cb = num(params.b, 200);
    const paletteName = String(params.palette ?? 'none');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    const level = clamp01(audio.level);
    const beat = audio.beat ?? 0;
    const audioMult = 1 + level * levelGain + beat * beatGain;

    const chars = text.length > 0 ? Array.from(text) : [' '];

    // Set a pixel via a permutation-keyed mapping. For 'planes' the (a, b)
    // axes are the glyph plane and `h` is the scroll axis; for 'ring' the
    // permutation isn't used directly (each face computes its own).
    const setIdx = (a: number, b: number, h: number): number => {
      if (axis === 'z') return idx(a, b, h);
      if (axis === 'y') return idx(a, h, b);
      return idx(h, a, b);
    };

    const writePixel = (i: number, weight: number, pr: number, pg: number, pb: number) => {
      if (weight <= 0) return;
      const off = i * 3;
      buffer[off] = Math.max(buffer[off], Math.round(pr * weight));
      buffer[off + 1] = Math.max(buffer[off + 1], Math.round(pg * weight));
      buffer[off + 2] = Math.max(buffer[off + 2], Math.round(pb * weight));
    };
    const glyphColor = (gx: number, gy: number): [number, number, number] => {
      if (!useP) return [cr, cg, cb];
      // Sample palette across the glyph diagonal so colored fonts don't
      // flatten to a single shade — gives a subtle gradient per char.
      const t01 = (gx + gy) / (FONT_W + FONT_H - 2);
      const [r0, g0, b0] = palette(clamp01(t01));
      return [r0, g0, b0];
    };

    if (mode === 'planes') {
      // Integrate scroll position; voxels/sec. `reverse` flips the sign so
      // characters travel the other way along the chosen axis.
      scrollPos += baseSpeed * audioMult * dt * (reverse ? -1 : 1);
      // Wrap to keep the number small over long runs.
      const totalLen = chars.length * charSpacing;
      if (totalLen > 0) {
        scrollPos = ((scrollPos % totalLen) + totalLen) % totalLen;
      }

      // Visible char index range. A char's slice lives at axis-position
      // (i * charSpacing - scrollPos). We need that in [-1, N] (allow a
      // sub-voxel margin so antialiased glyphs at the edges still show).
      const iMin = Math.floor((scrollPos - 1) / charSpacing);
      const iMax = Math.ceil((scrollPos + N) / charSpacing);

      // First pass: figure out which char is the "lead" (front of motion).
      // Without reverse, pos decreases over time so the front of motion is
      // the smallest in-view pos. With reverse, motion is the other way and
      // the front is the largest. leadAt='back' inverts that again.
      let leadI = NaN;
      if (highlightLead) {
        const wantSmallest = (leadAt === 'front') !== reverse;
        let bestPos = wantSmallest ? Infinity : -Infinity;
        for (let i = iMin; i <= iMax; i++) {
          const pos = i * charSpacing - scrollPos;
          if (pos < -1 || pos > N) continue;
          if ((wantSmallest && pos < bestPos) || (!wantSmallest && pos > bestPos)) {
            bestPos = pos;
            leadI = i;
          }
        }
      }

      for (let i = iMin; i <= iMax; i++) {
        const pos = i * charSpacing - scrollPos;
        if (pos < -1 || pos > N) continue;
        const ch = chars[((i % chars.length) + chars.length) % chars.length];
        const grid = getGlyph(ch);
        const charBright = highlightLead && i !== leadI ? trailDim : 1;

        // Antialias across two adjacent slices using fractional part — gives
        // smooth motion instead of a glyph snapping voxel-to-voxel.
        const h0 = Math.floor(pos);
        const fr = pos - h0;
        const splats: Array<{ h: number; w: number }> = [];
        if (h0 >= 0 && h0 < N) splats.push({ h: h0, w: 1 - fr });
        if (h0 + 1 >= 0 && h0 + 1 < N) splats.push({ h: h0 + 1, w: fr });

        for (let gy = 0; gy < FONT_H; gy++) {
          const row = grid[gy];
          for (let gx = 0; gx < FONT_W; gx++) {
            if (!row[gx]) continue;
            // Glyph row 0 = top, so vertical axis maps inverted: cube b = N-1-gy.
            const a = gx;
            const b = N - 1 - gy;
            const [pr, pg, pb] = glyphColor(gx, gy);
            for (const s of splats) {
              writePixel(setIdx(a, b, s.h), s.w * charBright, pr, pg, pb);
            }
          }
        }
      }
    } else if (mode === 'stack') {
      // Same character on every slice along the scroll axis. Each slice gets
      // its own color sampled from the palette by its axis position, so the
      // 10 stacked copies of the glyph form a colored gradient cube. Char
      // cycles over time at an audio-modulated rate.
      charPos += baseSpeed * audioMult * 0.5 * dt;
      if (chars.length > 0) {
        charPos = ((charPos % chars.length) + chars.length) % chars.length;
      }
      const i0 = Math.floor(charPos);
      const fr = charPos - i0;
      const fadeIn = fr < 0.85 ? 0 : (fr - 0.85) / 0.15;
      const showA = 1 - fadeIn;
      const showB = fadeIn;

      const drawStack = (ch: string, weight: number) => {
        if (weight <= 0) return;
        const grid = getGlyph(ch);
        for (let h = 0; h < N; h++) {
          // Per-plane color: palette sampled across the axis, or fall back
          // to the RGB color if no palette is selected. `reverse` flips the
          // gradient so palette(0) lands at the high end of the axis.
          const planeT = reverse ? 1 - h / (N - 1) : h / (N - 1);
          const [pr, pg, pb] = useP ? palette(planeT) : [cr, cg, cb];
          for (let gy = 0; gy < FONT_H; gy++) {
            const row = grid[gy];
            for (let gx = 0; gx < FONT_W; gx++) {
              if (!row[gx]) continue;
              const a = gx;
              const b = N - 1 - gy;
              writePixel(setIdx(a, b, h), weight, pr, pg, pb);
            }
          }
        }
      };

      drawStack(chars[i0 % chars.length], showA);
      if (showB > 0) drawStack(chars[(i0 + 1) % chars.length], showB);
    } else {
      // 'ring' mode: cycle through the text, drawing the current char on the
      // 4 vertical faces simultaneously.
      charPos += baseSpeed * audioMult * 0.5 * dt; // 0.5 → roughly 1.5 char/s default
      if (chars.length > 0) {
        charPos = ((charPos % chars.length) + chars.length) % chars.length;
      }
      const i0 = Math.floor(charPos);
      const fr = charPos - i0;
      // Cross-fade to next glyph as we transition. 0..0.85 = current only,
      // 0.85..1 = brief crossfade. Keeps each char readable for most of its
      // dwell time instead of dimming throughout.
      const fadeIn = fr < 0.85 ? 0 : (fr - 0.85) / 0.15;
      const showA = 1 - fadeIn;
      const showB = fadeIn;

      const drawChar = (ch: string, weight: number) => {
        if (weight <= 0) return;
        const grid = getGlyph(ch);
        // Iterate all 4 vertical faces. For each face pick the cube
        // permutation that puts vertical along `axis` and horizontal along
        // the face tangent.
        for (let face = 0; face < 4; face++) {
          for (let gy = 0; gy < FONT_H; gy++) {
            const row = grid[gy];
            for (let gx = clipL; gx < FONT_W - clipR; gx++) {
              if (!row[gx]) continue;
              // Map glyph (gx, gy) → cube (x, y, z) for this face & axis.
              // gy=0 is top of glyph → high end of vertical axis.
              const v = N - 1 - gy;
              const h = gx;
              let cx = 0, cy = 0, cz = 0;
              // Below uses 'axis' as the vertical direction in the cube.
              if (axis === 'z') {
                cz = v;
                if (face === 0) { cx = h; cy = 0; }
                else if (face === 1) { cx = N - 1; cy = h; }
                else if (face === 2) { cx = N - 1 - h; cy = N - 1; }
                else { cx = 0; cy = N - 1 - h; }
              } else if (axis === 'y') {
                cy = v;
                if (face === 0) { cx = h; cz = 0; }
                else if (face === 1) { cx = N - 1; cz = h; }
                else if (face === 2) { cx = N - 1 - h; cz = N - 1; }
                else { cx = 0; cz = N - 1 - h; }
              } else {
                cx = v;
                if (face === 0) { cy = h; cz = 0; }
                else if (face === 1) { cy = N - 1; cz = h; }
                else if (face === 2) { cy = N - 1 - h; cz = N - 1; }
                else { cy = 0; cz = N - 1 - h; }
              }
              const [pr, pg, pb] = glyphColor(gx, gy);
              writePixel(idx(cx, cy, cz), weight, pr, pg, pb);
            }
          }
        }
      };

      drawChar(chars[i0 % chars.length], showA);
      if (showB > 0) drawChar(chars[(i0 + 1) % chars.length], showB);
    }
  },
};
