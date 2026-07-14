import { CUBE_N } from '../types';
import { clamp01, hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * Square spiral: a rectangular spiral inscribed in each layer with a full
 * empty cell between arms (2-cell pitch), drawn head-first along its path
 * and then erased from the tail — the classic draw/undraw cycle. Layers are
 * phase-delayed so the animation corkscrews up the cube, and each layer can
 * be rotated a quarter-turn per step for a twisted-column look.
 */

const N = CUBE_N;

function buildSpiralPath(): [number, number][] {
  const cells: [number, number][] = [];
  let lo = 0;
  let hi = N - 1;
  let first = true;
  while (lo <= hi) {
    // Bottom arm left->right (with a connector cell from the outer ring).
    for (let x = first ? lo : lo - 1; x <= hi; x++) cells.push([x, lo]);
    // Right arm up.
    for (let y = lo + 1; y <= hi; y++) cells.push([hi, y]);
    if (hi > lo) {
      // Top arm right->left.
      for (let x = hi - 1; x >= lo; x--) cells.push([x, hi]);
      // Left arm down, stopping two short so the next ring keeps its gap.
      for (let y = hi - 1; y >= lo + 2; y--) cells.push([lo, y]);
    }
    first = false;
    lo += 2;
    hi -= 2;
  }
  return cells;
}

const PATH = buildSpiralPath();

let lastT = 0;
let phase = 0;

export const spiral: Pattern = {
  meta: {
    id: 'spiral',
    name: 'Spiral',
    description: 'Square spirals draw and unwind through the layers, corkscrewing up the cube.',
    params: [
      { key: 'axis', label: 'Layer axis', type: 'enum', options: ['z', 'y', 'x'], default: 'z' },
      { key: 'speed', label: 'Draw speed (cells/s)', type: 'number', min: 2, max: 60, step: 1, default: 18 },
      { key: 'layerDelay', label: 'Layer phase delay (s)', type: 'number', min: 0, max: 1, step: 0.02, default: 0.14 },
      { key: 'twist', label: 'Quarter-turns across height', type: 'number', min: 0, max: 4, step: 1, default: 1 },
      { key: 'tailDim', label: 'Oldest-cell brightness', type: 'number', min: 0.1, max: 1, step: 0.05, default: 0.45 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'cyberpunk' },
      { key: 'colorBy', label: 'Color by', type: 'enum', options: ['position', 'layer'], default: 'position' },
      { key: 'levelGain', label: 'Level → speed', type: 'number', min: 0, max: 4, step: 0.1, default: 1.0 },
      { key: 'sat', label: 'Saturation (HSV mode)', type: 'number', min: 0, max: 1, step: 0.05, default: 0.9 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    phase = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const axis = String(params.axis ?? 'z');
    const speed = num(params.speed, 18);
    const layerDelay = num(params.layerDelay, 0.14);
    const twist = Math.max(0, Math.floor(num(params.twist, 1)));
    const tailDim = clamp01(num(params.tailDim, 0.45));
    const levelGain = num(params.levelGain, 1.0);
    const sat = num(params.sat, 0.9);
    const colorByLayer = String(params.colorBy ?? 'position') === 'layer';
    const paletteName = String(params.palette ?? 'cyberpunk');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    const len = PATH.length;
    const cycle = 2 * len;
    // Integrated phase so audio speed boosts don't jump the animation.
    phase += speed * (1 + (audio.level ?? 0) * levelGain) * dt;

    const setIdx = (a: number, b: number, h: number): number => {
      if (axis === 'z') return idx(a, b, h);
      if (axis === 'y') return idx(a, h, b);
      return idx(h, a, b);
    };

    for (let h = 0; h < N; h++) {
      const p = ((phase - (h * layerDelay * speed)) % cycle + cycle) % cycle;
      // Draw phase: cells [0, p); erase phase: cells [p-len, len).
      const drawing = p < len;
      const from = drawing ? 0 : Math.floor(p - len);
      const to = drawing ? Math.floor(p) : len;
      if (to <= from) continue;

      // Quarter-turn rotation per layer for the twist.
      const turns = Math.floor((twist * h) / N) % 4;

      for (let i = from; i < to; i++) {
        let [cx, cy] = PATH[i];
        for (let r = 0; r < turns; r++) {
          const nx = cy;
          const ny = N - 1 - cx;
          cx = nx;
          cy = ny;
        }
        // Newest cells full brightness, fading toward the oldest lit cell.
        const age = (to - 1 - i) / Math.max(1, to - from);
        const k = 1 - age * (1 - tailDim);
        const t01 = colorByLayer ? h / (N - 1) : i / (len - 1);
        let r: number, g: number, b: number;
        if (useP) [r, g, b] = palette(t01);
        else [r, g, b] = hsvToRgb(t01 * 0.8, sat, 1);
        const o = setIdx(cx, cy, h) * 3;
        buffer[o] = Math.max(buffer[o], Math.round(r * k));
        buffer[o + 1] = Math.max(buffer[o + 1], Math.round(g * k));
        buffer[o + 2] = Math.max(buffer[o + 2], Math.round(b * k));
      }
    }
  },
};
