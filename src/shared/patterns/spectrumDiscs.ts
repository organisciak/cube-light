import { AUDIO_BANDS, CUBE_N } from '../types';
import { clamp01, hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * Stack 10 slices along a chosen axis and assign each slice a different
 * frequency band. Within a slice, draw an antialiased filled disc centered
 * on the slice — radius grows with that band's energy. Quiet bands show a
 * tiny dot near the center; loud bands fill the whole slice. Color per
 * slice is taken from the palette so the spectrum reads as a gradient
 * across the cube.
 */

let smoothed: number[] = new Array(CUBE_N).fill(0);

export const spectrumDiscs: Pattern = {
  meta: {
    id: 'spectrum-discs',
    name: 'Spectrum Discs',
    description: 'Each slice along the chosen axis is one frequency band; band energy expands a disc out from the slice center.',
    params: [
      { key: 'axis', label: 'Stack axis', type: 'enum', options: ['x', 'y', 'z'], default: 'y' },
      { key: 'minRadius', label: 'Minimum radius', type: 'number', min: 0, max: 4, step: 0.1, default: 1 },
      { key: 'gain', label: 'Audio gain', type: 'number', min: 0.1, max: 4, step: 0.1, default: 1.5 },
      { key: 'gamma', label: 'Response curve', type: 'number', min: 0.3, max: 3, step: 0.05, default: 0.7 },
      { key: 'edge', label: 'Edge softness', type: 'number', min: 0.2, max: 2, step: 0.1, default: 0.7 },
      { key: 'attack', label: 'Attack (s)', type: 'number', min: 0, max: 0.5, step: 0.01, default: 0.04 },
      { key: 'release', label: 'Release (s)', type: 'number', min: 0.02, max: 1.5, step: 0.02, default: 0.25 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'spectrum' },
      { key: 'sat', label: 'Saturation (HSV mode)', type: 'number', min: 0, max: 1, step: 0.05, default: 0.85 },
      { key: 'floor', label: 'Brightness floor', type: 'number', min: 0, max: 1, step: 0.02, default: 0.1 },
      { key: 'beatBoost', label: 'Beat boost', type: 'number', min: 0, max: 1, step: 0.05, default: 0.25 },
    ],
  },
  init(ctx) {
    smoothed = new Array(CUBE_N).fill(0);
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, params, audio, dt } = ctx;
    const N = CUBE_N;
    const axis = (String(params.axis ?? 'y') as 'x' | 'y' | 'z');
    const minRadius = Math.max(0, num(params.minRadius, 1));
    const gain = num(params.gain, 1.5);
    const gamma = Math.max(0.1, num(params.gamma, 0.7));
    const edge = Math.max(0.1, num(params.edge, 0.7));
    const attack = Math.max(0, num(params.attack, 0.04));
    const release = Math.max(0.01, num(params.release, 0.25));
    const sat = num(params.sat, 0.85);
    const floorAmt = clamp01(num(params.floor, 0.1));
    const beatBoost = clamp01(num(params.beatBoost, 0.25));
    const paletteName = String(params.palette ?? 'spectrum');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    const center = (N - 1) / 2;
    // Max possible radius is the corner distance from center.
    const cornerR = Math.hypot(center, center);
    const maxRadius = cornerR + 0.5;

    const beat = audio.beat ?? 0;

    // Build per-slice target energies by mapping slice index 0..N-1 onto the
    // band axis 0..AUDIO_BANDS-1 (linear interp between adjacent bands).
    for (let i = 0; i < N; i++) {
      const bf = (i / (N - 1)) * (AUDIO_BANDS - 1);
      const b0 = Math.floor(bf);
      const b1 = Math.min(AUDIO_BANDS - 1, b0 + 1);
      const f = bf - b0;
      const v0 = audio.bands[b0] ?? 0;
      const v1 = audio.bands[b1] ?? 0;
      const target = clamp01((v0 * (1 - f) + v1 * f) * gain);
      // Asymmetric smoothing: faster on rising energy than on falling.
      const tau = target > smoothed[i] ? attack : release;
      const k = tau <= 0 ? 1 : 1 - Math.exp(-dt / tau);
      smoothed[i] += (target - smoothed[i]) * k;
    }

    // Index helper that swaps the stack axis for the slice index.
    const getIdx = (slice: number, a: number, b: number): number => {
      // a, b are coordinates along the two non-stack axes, in canonical order:
      //   axis=x → (a,b) = (y,z)
      //   axis=y → (a,b) = (x,z)
      //   axis=z → (a,b) = (x,y)
      if (axis === 'x') return idx(slice, a, b);
      if (axis === 'y') return idx(a, slice, b);
      return idx(a, b, slice);
    };

    buffer.fill(0);

    for (let s = 0; s < N; s++) {
      const energy = clamp01(smoothed[s] + beatBoost * beat);
      // Map energy through gamma so quieter sounds still register visibly.
      const shaped = Math.pow(energy, gamma);
      const radius = minRadius + (maxRadius - minRadius) * shaped;

      // Color for this slice from the palette (or HSV fallback).
      const pos = s / (N - 1);
      let cr: number, cg: number, cb: number;
      if (useP) {
        const [r, g, b] = palette(pos);
        cr = r; cg = g; cb = b;
      } else {
        const [r, g, b] = hsvToRgb(pos, sat, 1);
        cr = r; cg = g; cb = b;
      }

      // Per-slice brightness: include a small floor so quiet slices still
      // glow faintly, plus the energy itself so loud bands are brighter.
      const brightness = clamp01(floorAmt + (1 - floorAmt) * shaped);

      for (let a = 0; a < N; a++) {
        for (let b = 0; b < N; b++) {
          const dx = a - center;
          const dy = b - center;
          const d = Math.hypot(dx, dy);
          // Antialiased disc: 1 inside, fades over `edge` voxels at the rim.
          const cover = clamp01((radius - d) / edge + 0.5);
          if (cover <= 0) continue;
          const v = cover * brightness;
          const i = getIdx(s, a, b) * 3;
          buffer[i] = Math.round(cr * v);
          buffer[i + 1] = Math.round(cg * v);
          buffer[i + 2] = Math.round(cb * v);
        }
      }
    }
  },
};
