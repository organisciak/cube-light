import { CUBE_N } from '../types';
import { clamp01, hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * Scanner: a thin bright plane sweeps along an axis; each perpendicular slice
 * flares as the plane crosses it, then fades — a Cylon/CT-scanner sweep. The
 * trail is stored as a per-slice "last hit" phase so only a couple of slices
 * glow at once, keeping it sparse. Bounces at the ends. Beats can invert the
 * sweep direction for a stutter effect.
 */

const N = CUBE_N;

let lastT = 0;
let pos = 0;
let dir = 1;
const hitPhase: number[] = new Array(N).fill(-99); // ctx.t when each slice was last crossed

export const scan: Pattern = {
  meta: {
    id: 'scan',
    name: 'Scanner',
    description: 'A bright plane sweeps through the cube; each slice flares then fades. Cylon sweep.',
    params: [
      { key: 'axis', label: 'Sweep axis', type: 'enum', options: ['z', 'y', 'x'], default: 'z' },
      { key: 'speed', label: 'Sweep speed (slices/s)', type: 'number', min: 2, max: 40, step: 1, default: 11 },
      { key: 'fade', label: 'Slice fade time (s)', type: 'number', min: 0.1, max: 2, step: 0.05, default: 0.55 },
      { key: 'gridOnly', label: 'Grid lines only (sparser)', type: 'bool', default: false },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'arctic' },
      { key: 'colorBySlice', label: 'Color follows slice', type: 'bool', default: true },
      { key: 'sat', label: 'Saturation (HSV mode)', type: 'number', min: 0, max: 1, step: 0.05, default: 0.85 },
      { key: 'levelGain', label: 'Level → speed', type: 'number', min: 0, max: 4, step: 0.1, default: 1 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    pos = 0; dir = 1;
    hitPhase.fill(ctx.t - 99);
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const axis = String(params.axis ?? 'z');
    const speed = num(params.speed, 11);
    const fade = Math.max(0.05, num(params.fade, 0.55));
    const gridOnly = Boolean(params.gridOnly ?? false);
    const colorBySlice = Boolean(params.colorBySlice ?? true);
    const sat = num(params.sat, 0.85);
    const levelGain = num(params.levelGain, 1);
    const paletteName = String(params.palette ?? 'arctic');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    // Advance the sweep, bouncing at the ends. Stamp each integer slice the
    // plane crosses with the current time.
    const prev = pos;
    pos += dir * speed * (1 + (audio.level ?? 0) * levelGain) * dt;
    if (pos > N - 1) { pos = N - 1 - (pos - (N - 1)); dir = -1; }
    else if (pos < 0) { pos = -pos; dir = 1; }
    const from = Math.min(prev, pos), to = Math.max(prev, pos);
    for (let s = Math.ceil(from); s <= Math.floor(to); s++) {
      if (s >= 0 && s < N) hitPhase[s] = t;
    }

    const setIdx = (slice: number, a: number, b: number): number => {
      if (axis === 'z') return idx(a, b, slice);
      if (axis === 'y') return idx(a, slice, b);
      return idx(slice, a, b);
    };

    for (let s = 0; s < N; s++) {
      const age = t - hitPhase[s];
      if (age > fade) continue;
      const env = clamp01(1 - age / fade);
      const bright = env * env; // quadratic fade reads snappier
      const tCol = colorBySlice ? s / (N - 1) : 0.5;
      let cr: number, cg: number, cb: number;
      if (useP) [cr, cg, cb] = palette(tCol);
      else [cr, cg, cb] = hsvToRgb(0.55 - tCol * 0.4, sat, 1);

      for (let a = 0; a < N; a++) {
        for (let b = 0; b < N; b++) {
          // Grid mode: only the slice's border + a cross through the middle,
          // for a much sparser "scan frame" look.
          if (gridOnly) {
            const edge = a === 0 || a === N - 1 || b === 0 || b === N - 1;
            const cross = a === Math.floor(N / 2) || b === Math.floor(N / 2);
            if (!edge && !cross) continue;
          }
          const i = setIdx(s, a, b) * 3;
          buffer[i] = Math.round(cr * bright);
          buffer[i + 1] = Math.round(cg * bright);
          buffer[i + 2] = Math.round(cb * bright);
        }
      }
    }
  },
};
