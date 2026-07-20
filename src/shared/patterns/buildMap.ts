import { CUBE_N } from '../types';
import type { Pattern } from './types';

/**
 * Build / assembly helper. Lights LEDs by RAW WIRE INDEX (not by coordinate),
 * so it works before the cube is calibrated — during physical assembly the
 * geometry mapping isn't solved yet.
 *
 * Each physical strand of the cube is `period` LEDs long (10 for a 10×10×10).
 * This marks:
 *   - the START of every strand (wire offset 0) bright in the "end" color, with
 *     an optional dim second LED (offset 1) so you can read the wire direction —
 *     i.e. indices 0, (1), 10, 11, 20, 21, …
 *   - the CENTER of every strand (offsets `centerOffset` and +1, default 5/6) in
 *     a different color, so strand centers can be lined up as they're threaded.
 *
 * Writes straight into the 1000-LED buffer, so both data pins (the 2×500 split)
 * are covered automatically.
 */

const num = (v: unknown, d: number): number =>
  typeof v === 'number' && Number.isFinite(v) ? v : d;
const bool = (v: unknown, d: boolean): boolean => (typeof v === 'boolean' ? v : d);

const N = CUBE_N * CUBE_N * CUBE_N;

export const buildMap: Pattern = {
  meta: {
    id: 'build-map',
    name: 'Build Map (Assembly)',
    description:
      'Marks each strand start (raw wire index 0/1) and center (5/6) by index — a physical build aid. Use the Calibrate tab.',
    params: [
      { key: 'period', label: 'Strand length (LEDs)', type: 'number', min: 2, max: 100, step: 1, default: 10 },
      { key: 'endR', label: 'End R', type: 'number', min: 0, max: 255, step: 1, default: 0 },
      { key: 'endG', label: 'End G', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'endB', label: 'End B', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'dimDir', label: 'Dim 2nd LED (show direction)', type: 'bool', default: true },
      { key: 'showCenter', label: 'Mark strand centers', type: 'bool', default: true },
      { key: 'centerOffset', label: 'Center offset in strand', type: 'number', min: 0, max: 99, step: 1, default: 5 },
      { key: 'ctrR', label: 'Center R', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'ctrG', label: 'Center G', type: 'number', min: 0, max: 255, step: 1, default: 90 },
      { key: 'ctrB', label: 'Center B', type: 'number', min: 0, max: 255, step: 1, default: 0 },
    ],
  },
  render(ctx) {
    ctx.buffer.fill(0);
    const period = Math.max(2, Math.floor(num(ctx.params.period, 10)));
    const endR = num(ctx.params.endR, 0);
    const endG = num(ctx.params.endG, 255);
    const endB = num(ctx.params.endB, 255);
    const dimDir = bool(ctx.params.dimDir, true);
    const showCenter = bool(ctx.params.showCenter, true);
    const centerOffset = Math.max(0, Math.floor(num(ctx.params.centerOffset, 5)));
    const ctrR = num(ctx.params.ctrR, 255);
    const ctrG = num(ctx.params.ctrG, 90);
    const ctrB = num(ctx.params.ctrB, 0);

    const put = (i: number, r: number, g: number, b: number): void => {
      if (i < 0 || i >= N) return;
      const o = i * 3;
      ctx.buffer[o] = r;
      ctx.buffer[o + 1] = g;
      ctx.buffer[o + 2] = b;
    };

    for (let base = 0; base < N; base += period) {
      put(base, endR, endG, endB);
      if (dimDir) put(base + 1, Math.round(endR * 0.28), Math.round(endG * 0.28), Math.round(endB * 0.28));
      if (showCenter && centerOffset < period) {
        put(base + centerOffset, ctrR, ctrG, ctrB);
        if (centerOffset + 1 < period) put(base + centerOffset + 1, ctrR, ctrG, ctrB);
      }
    }
  },
};
