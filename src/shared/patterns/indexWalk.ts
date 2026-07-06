import { NUM_LEDS } from '../types';
import type { Pattern } from './types';

/**
 * Walk a single bright pixel through the LED index in order.
 * Use this to physically map LED index -> (x,y,z) when calibrating layout.
 */
export const indexWalk: Pattern = {
  meta: {
    id: 'index-walk',
    name: 'Index Walk',
    description: 'Lights LEDs one-by-one in wire order. Use to calibrate layout.',
    params: [
      { key: 'speed', label: 'LEDs/sec', type: 'number', min: 1, max: 200, step: 1, default: 25 },
      { key: 'tail', label: 'Tail length', type: 'number', min: 0, max: 50, step: 1, default: 6 },
    ],
  },
  render(ctx) {
    const speed = (ctx.params.speed as number) ?? 25;
    const tail = Math.max(0, Math.floor((ctx.params.tail as number) ?? 6));
    ctx.buffer.fill(0);
    const head = Math.floor(ctx.t * speed) % NUM_LEDS;
    for (let k = 0; k <= tail; k++) {
      const i = (head - k + NUM_LEDS) % NUM_LEDS;
      const v = Math.round(255 * (1 - k / (tail + 1)));
      const o = i * 3;
      ctx.buffer[o] = v;
      ctx.buffer[o + 1] = v;
      ctx.buffer[o + 2] = v;
    }
  },
};
