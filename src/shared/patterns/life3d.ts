import { CUBE_N } from '../types';
import { num } from '../color';
import { getPalette } from '../palettes';
import type { Pattern } from './types';

/**
 * 3D Conway-style cellular automaton. The default rule is Carter Bays's
 * 5766 (alive cells survive with 5..7 live neighbours; dead cells are born
 * with exactly 6) which gives sustained, varied activity on a small grid.
 * Toroidal wrap on all axes. Auto-reseeds when the population dies or when
 * the same population count persists for too long.
 */
const N = CUBE_N;
const TOTAL = N * N * N;

let grid: Uint8Array | null = null;
let next: Uint8Array | null = null;
let age: Uint16Array | null = null;
let lastStepT = 0;
let prevPopulation = -1;
let stagnationFrames = 0;

function reseed(density: number) {
  grid = grid ?? new Uint8Array(TOTAL);
  age = age ?? new Uint16Array(TOTAL);
  for (let i = 0; i < TOTAL; i++) {
    grid[i] = Math.random() < density ? 1 : 0;
    age[i] = grid[i] ? 1 : 0;
  }
}

function indexAt(x: number, y: number, z: number): number {
  return ((z + N) % N) * N * N + ((y + N) % N) * N + ((x + N) % N);
}

function parseRule(rule: string): { survive: Set<number>; born: Set<number> } {
  // Format examples: "5,6,7/6" or "5-7/6-6" — a slash separates survive from born.
  const [s = '', b = ''] = rule.split('/');
  const parse = (chunk: string): Set<number> => {
    const out = new Set<number>();
    for (const part of chunk.split(',').map((p) => p.trim()).filter(Boolean)) {
      const m = /^(\d+)(?:-(\d+))?$/.exec(part);
      if (!m) continue;
      const lo = Number(m[1]);
      const hi = m[2] != null ? Number(m[2]) : lo;
      for (let n = lo; n <= hi; n++) out.add(n);
    }
    return out;
  };
  return { survive: parse(s), born: parse(b) };
}

export const life3d: Pattern = {
  meta: {
    id: 'life-3d',
    name: '3D Life',
    description: '3D cellular automaton on a 10×10×10 toroidal grid. Default rule: Bays 5766.',
    params: [
      { key: 'rule', label: 'Rule (S/B)', type: 'enum', options: ['5-7/6', '4/5-7', '5/4-6', '4-5/5', '6-8/5-7'], default: '5-7/6' },
      { key: 'stepHz', label: 'Steps per second', type: 'number', min: 1, max: 30, step: 0.5, default: 6 },
      { key: 'density', label: 'Seed density', type: 'number', min: 0.1, max: 0.6, step: 0.01, default: 0.32 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'spectrum' },
      { key: 'fadeAge', label: 'Cell fade-in age', type: 'number', min: 1, max: 30, step: 1, default: 6 },
    ],
  },
  init(ctx) {
    grid = new Uint8Array(TOTAL);
    next = new Uint8Array(TOTAL);
    age = new Uint16Array(TOTAL);
    reseed(num(ctx.params.density, 0.32));
    lastStepT = ctx.t;
    prevPopulation = -1;
    stagnationFrames = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    if (!grid || !next || !age) {
      grid = new Uint8Array(TOTAL);
      next = new Uint8Array(TOTAL);
      age = new Uint16Array(TOTAL);
      reseed(num(ctx.params.density, 0.32));
      lastStepT = ctx.t;
    }
    const { buffer, idx, t, params } = ctx;
    const ruleStr = (params.rule as string) || '5-7/6';
    const { survive, born } = parseRule(ruleStr);
    const stepHz = num(params.stepHz, 6);
    const stepInterval = 1 / Math.max(0.5, stepHz);
    const palette = getPalette(params.palette as string);
    const fadeAge = Math.max(1, num(params.fadeAge, 6));

    // Step the simulation if enough time has passed.
    if (t - lastStepT >= stepInterval) {
      lastStepT = t;
      let pop = 0;
      for (let z = 0; z < N; z++) {
        for (let y = 0; y < N; y++) {
          for (let x = 0; x < N; x++) {
            // Count Moore-26 neighbours.
            let c = 0;
            for (let dz = -1; dz <= 1; dz++) {
              for (let dy = -1; dy <= 1; dy++) {
                for (let dx = -1; dx <= 1; dx++) {
                  if (dx === 0 && dy === 0 && dz === 0) continue;
                  c += grid[indexAt(x + dx, y + dy, z + dz)];
                }
              }
            }
            const here = grid[indexAt(x, y, z)];
            const willLive = here ? survive.has(c) : born.has(c);
            const i = indexAt(x, y, z);
            next[i] = willLive ? 1 : 0;
            if (willLive) {
              age[i] = (age[i] ?? 0) + 1;
              pop++;
            } else {
              age[i] = 0;
            }
          }
        }
      }
      // Swap grids.
      const tmp = grid;
      grid = next;
      next = tmp;

      // Detect stagnation: same population count for many ticks → reseed.
      if (pop === prevPopulation) stagnationFrames++;
      else stagnationFrames = 0;
      prevPopulation = pop;
      if (pop === 0 || stagnationFrames > 60) {
        reseed(num(params.density, 0.32));
        prevPopulation = -1;
        stagnationFrames = 0;
      }
    }

    // Render: alive cells colored by palette using their normalized age.
    for (let z = 0; z < N; z++) {
      for (let y = 0; y < N; y++) {
        for (let x = 0; x < N; x++) {
          const gi = indexAt(x, y, z);
          if (!grid[gi]) {
            const i = idx(x, y, z) * 3;
            buffer[i] = 0;
            buffer[i + 1] = 0;
            buffer[i + 2] = 0;
            continue;
          }
          const fade = Math.min(1, age[gi] / fadeAge);
          // Palette ramps with normalized z so cells in different layers vary in hue.
          const t01 = (z / (N - 1)) * 0.7 + 0.3 * fade;
          const [r, g, b] = palette(t01);
          const i = idx(x, y, z) * 3;
          buffer[i] = Math.round(r * (0.4 + 0.6 * fade));
          buffer[i + 1] = Math.round(g * (0.4 + 0.6 * fade));
          buffer[i + 2] = Math.round(b * (0.4 + 0.6 * fade));
        }
      }
    }
  },
};
