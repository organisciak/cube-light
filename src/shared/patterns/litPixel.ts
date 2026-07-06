import type { Pattern } from './types';

/**
 * Lights exactly one LED bright white (by raw LED index, not by coordinate).
 * Used by the calibration wizard.
 */
export const litPixel: Pattern = {
  meta: {
    id: 'lit-pixel',
    name: 'Single Pixel (Calibration)',
    description: 'Lights one LED by raw index. Use the Calibration tab.',
    params: [
      { key: 'ledIdx', label: 'LED index', type: 'number', min: 0, max: 999, step: 1, default: 0 },
      { key: 'r', label: 'R', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'g', label: 'G', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'b', label: 'B', type: 'number', min: 0, max: 255, step: 1, default: 255 },
    ],
  },
  render(ctx) {
    ctx.buffer.fill(0);
    const idx = Math.max(0, Math.min(999, Math.floor(num(ctx.params.ledIdx, 0))));
    const r = num(ctx.params.r, 255);
    const g = num(ctx.params.g, 255);
    const b = num(ctx.params.b, 255);
    const o = idx * 3;
    ctx.buffer[o] = r;
    ctx.buffer[o + 1] = g;
    ctx.buffer[o + 2] = b;
  },
};

function num(v: unknown, d: number): number {
  return typeof v === 'number' && Number.isFinite(v) ? v : d;
}
