import { CUBE_N } from '../types';
import type { Pattern } from './types';

/**
 * Build / assembly helper — two modes:
 *
 * "axis" (geometry): on a chosen axis, light the two extreme planes (coord 0 and
 *   N-1) across every layer, plus the two middle planes (N/2-1, N/2) in a second
 *   color. Uses ctx.idx, so it follows the calibrated layout/orientation — the lit
 *   faces are the actual physical faces of the cube. Use it to verify a layout or
 *   to orient the assembled cube.
 *
 * "strand" (raw wire index, no calibration needed): each physical strand is
 *   `period` LEDs long (10 for a 10×10×10). Marks each strand's two ENDS —
 *   wire offset 0 and period-1, i.e. indices 0, 9,10, 19,20, 29,30, … — where
 *   consecutive strands fold together, plus the two middle LEDs of each strand
 *   in a second color. Good for identifying/threading strands during the build,
 *   before the cube is calibrated.
 *
 * Both write into the 1000-LED buffer, so both data pins (the 2×500 split) are
 * covered automatically.
 */

const num = (v: unknown, d: number): number =>
  typeof v === 'number' && Number.isFinite(v) ? v : d;
const bool = (v: unknown, d: boolean): boolean => (typeof v === 'boolean' ? v : d);

const N = CUBE_N;
const TOTAL = N * N * N;

export const buildMap: Pattern = {
  meta: {
    id: 'build-map',
    name: 'Build Map (Assembly)',
    description:
      'Highlights cube faces (axis mode) or strand ends by wire index (strand mode) — a physical build aid. Use the Calibrate tab.',
    params: [
      { key: 'mode', label: 'Mode', type: 'enum', options: ['axis', 'strand'], default: 'axis' },
      { key: 'axis', label: 'Axis (axis mode)', type: 'enum', options: ['x', 'y', 'z'], default: 'x' },
      { key: 'period', label: 'Strand length (strand mode)', type: 'number', min: 2, max: 100, step: 1, default: 10 },
      { key: 'showCenter', label: 'Mark the two middle', type: 'bool', default: true },
      { key: 'endR', label: 'End R', type: 'number', min: 0, max: 255, step: 1, default: 0 },
      { key: 'endG', label: 'End G', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'endB', label: 'End B', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'ctrR', label: 'Middle R', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'ctrG', label: 'Middle G', type: 'number', min: 0, max: 255, step: 1, default: 90 },
      { key: 'ctrB', label: 'Middle B', type: 'number', min: 0, max: 255, step: 1, default: 0 },
    ],
  },
  render(ctx) {
    ctx.buffer.fill(0);
    const mode = String(ctx.params.mode ?? 'axis');
    const showCenter = bool(ctx.params.showCenter, true);
    const endR = num(ctx.params.endR, 0);
    const endG = num(ctx.params.endG, 255);
    const endB = num(ctx.params.endB, 255);
    const ctrR = num(ctx.params.ctrR, 255);
    const ctrG = num(ctx.params.ctrG, 90);
    const ctrB = num(ctx.params.ctrB, 0);

    const putRaw = (i: number, r: number, g: number, b: number): void => {
      if (i < 0 || i >= TOTAL) return;
      const o = i * 3;
      ctx.buffer[o] = r;
      ctx.buffer[o + 1] = g;
      ctx.buffer[o + 2] = b;
    };

    if (mode === 'strand') {
      const period = Math.max(2, Math.floor(num(ctx.params.period, 10)));
      // Ends of every strand: offset 0 and offset period-1 (the folds).
      for (let i = 0; i < TOTAL; i++) {
        const off = i % period;
        if (off === 0 || off === period - 1) putRaw(i, endR, endG, endB);
      }
      if (showCenter) {
        const midLo = Math.floor((period - 1) / 2);
        const midHi = midLo + 1;
        for (let base = 0; base < TOTAL; base += period) {
          putRaw(base + midLo, ctrR, ctrG, ctrB);
          if (midHi < period) putRaw(base + midHi, ctrR, ctrG, ctrB);
        }
      }
      return;
    }

    // axis mode: light whole planes at a coordinate on the chosen axis.
    const axis = String(ctx.params.axis ?? 'x');
    const plane = (c: number, r: number, g: number, b: number): void => {
      for (let u = 0; u < N; u++) {
        for (let v = 0; v < N; v++) {
          const i = axis === 'x' ? ctx.idx(c, u, v) : axis === 'y' ? ctx.idx(u, c, v) : ctx.idx(u, v, c);
          const o = i * 3;
          ctx.buffer[o] = r;
          ctx.buffer[o + 1] = g;
          ctx.buffer[o + 2] = b;
        }
      }
    };
    plane(0, endR, endG, endB);
    plane(N - 1, endR, endG, endB);
    if (showCenter) {
      const midLo = Math.floor((N - 1) / 2);
      plane(midLo, ctrR, ctrG, ctrB);
      plane(midLo + 1, ctrR, ctrG, ctrB);
    }
  },
};
