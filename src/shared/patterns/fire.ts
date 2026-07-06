import { CUBE_N } from '../types';
import { num } from '../color';
import { getPalette, paletteNames } from '../palettes';
import type { Pattern } from './types';

/**
 * 3D Fire2012-style flame rising from z=0.
 * Each frame: cool every cell, drift heat upward (averaging the two below),
 * spawn random sparks at the bottom layer. Audio level + beat boost spark rate.
 * Heat 0..255 maps to the chosen palette ('fire' by default).
 */
let heat: Float32Array | null = null;
let lastT = 0;

export const fire: Pattern = {
  meta: {
    id: 'fire',
    name: 'Fire',
    description: 'Heat rises from the base, cools as it climbs. Audio fans the flames.',
    params: [
      { key: 'cooling', label: 'Cooling', type: 'number', min: 0.5, max: 4, step: 0.05, default: 1.4 },
      { key: 'sparking', label: 'Sparking rate', type: 'number', min: 0, max: 1, step: 0.01, default: 0.55 },
      { key: 'sparkHeat', label: 'Spark heat', type: 'number', min: 80, max: 255, step: 5, default: 200 },
      { key: 'baseLayers', label: 'Spark zone (z layers)', type: 'number', min: 1, max: 4, step: 1, default: 2 },
      { key: 'driftRate', label: 'Drift rate', type: 'number', min: 0.3, max: 1, step: 0.01, default: 0.85 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'fire' },
      { key: 'audioGain', label: 'Audio fan', type: 'number', min: 0, max: 5, step: 0.1, default: 2 },
    ],
  },
  init(ctx) {
    heat = new Float32Array(CUBE_N * CUBE_N * CUBE_N);
    lastT = ctx.t;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    if (!heat) heat = new Float32Array(CUBE_N * CUBE_N * CUBE_N);
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const cooling = num(params.cooling, 1.4);
    const sparking = num(params.sparking, 0.55);
    const sparkHeat = num(params.sparkHeat, 200);
    const baseLayers = Math.max(1, Math.floor(num(params.baseLayers, 2)));
    const drift = Math.min(1, num(params.driftRate, 0.85));
    const palette = getPalette(params.palette as string);
    const audioGain = num(params.audioGain, 2);

    // Effective frame step: scale by 30fps reference so behaviour is FPS-stable.
    const fStep = Math.min(2, dt * 30);

    // Index helper into the heat field (column-major in z is fine; we just want each cell).
    const H = (x: number, y: number, z: number) => (z * N + y) * N + x;

    // 1) Cool every cell.
    for (let i = 0; i < heat.length; i++) {
      heat[i] = Math.max(0, heat[i] - (Math.random() * cooling + 0.4) * fStep);
    }

    // 2) Heat drifts upward. Walk z from top to bottom, replacing each cell
    //    with a weighted average of itself and the two cells below.
    for (let z = N - 1; z >= 2; z--) {
      for (let y = 0; y < N; y++) {
        for (let x = 0; x < N; x++) {
          const a = heat[H(x, y, z - 1)];
          const b = heat[H(x, y, z - 2)];
          const c = heat[H(x, y, z)];
          heat[H(x, y, z)] = c * (1 - drift) + (a + b + a) / 3 * drift;
        }
      }
    }

    // 3) Sparks at the base, biased by audio level + beat.
    const audioBoost = 1 + audio.level * audioGain + audio.beat * 1.5;
    for (let y = 0; y < N; y++) {
      for (let x = 0; x < N; x++) {
        if (Math.random() < sparking * audioBoost * fStep * 0.6) {
          const z = Math.floor(Math.random() * baseLayers);
          const add = sparkHeat * (0.6 + Math.random() * 0.4);
          heat[H(x, y, z)] = Math.min(255, heat[H(x, y, z)] + add);
        }
      }
    }

    // 4) Render heat → color via palette.
    for (let z = 0; z < N; z++) {
      for (let y = 0; y < N; y++) {
        for (let x = 0; x < N; x++) {
          const t01 = Math.max(0, Math.min(1, heat[H(x, y, z)] / 255));
          const [r, g, b] = palette(t01);
          const i = idx(x, y, z) * 3;
          buffer[i] = r;
          buffer[i + 1] = g;
          buffer[i + 2] = b;
        }
      }
    }
  },
};

// Touch palettes import to make the bundler include it for the 'palette' param UI.
void paletteNames;
