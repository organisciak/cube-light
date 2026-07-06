import { NUM_LEDS } from '../types';
import type { Pattern } from './types';

export const solid: Pattern = {
  meta: {
    id: 'solid',
    name: 'Solid',
    description: 'Useful for sanity-checking wiring.',
    params: [
      { key: 'r', label: 'R', type: 'number', min: 0, max: 255, step: 1, default: 64 },
      { key: 'g', label: 'G', type: 'number', min: 0, max: 255, step: 1, default: 64 },
      { key: 'b', label: 'B', type: 'number', min: 0, max: 255, step: 1, default: 64 },
    ],
  },
  render(ctx) {
    const r = (ctx.params.r as number) ?? 64;
    const g = (ctx.params.g as number) ?? 64;
    const b = (ctx.params.b as number) ?? 64;
    for (let i = 0; i < NUM_LEDS; i++) {
      const o = i * 3;
      ctx.buffer[o] = r;
      ctx.buffer[o + 1] = g;
      ctx.buffer[o + 2] = b;
    }
  },
};
