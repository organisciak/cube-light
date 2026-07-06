import { CUBE_N, NUM_LEDS } from '../types';
import { clamp01, hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

interface Drop {
  x: number;
  y: number;
  z: number;
  vz: number;
  /** Palette position 0..1 (also used as HSV hue when palette is 'none'). */
  pos: number;
  brightness: number;
}

let drops: Drop[] = [];
let lastT = 0;
let pendingSpawnFrac = 0;

/**
 * Rain: drops spawn at the top of the cube and fall along z, leaving fading
 * trails (achieved by decaying the buffer each frame, then writing the head).
 * Audio level multiplies the spawn rate, so heavy bass = downpour.
 */
export const rain: Pattern = {
  meta: {
    id: 'rain',
    name: 'Rain',
    description: 'Drops spawn at the top and fall, leaving fading trails. Audio boosts spawn rate.',
    params: [
      { key: 'spawnRate', label: 'Spawn rate (drops/s)', type: 'number', min: 0, max: 80, step: 1, default: 14 },
      { key: 'fallSpeed', label: 'Fall speed (z/s)', type: 'number', min: 1, max: 30, step: 0.5, default: 9 },
      { key: 'speedJitter', label: 'Speed jitter', type: 'number', min: 0, max: 1, step: 0.05, default: 0.4 },
      { key: 'trail', label: 'Trail decay (per frame)', type: 'number', min: 0.5, max: 0.99, step: 0.01, default: 0.85 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'arctic' },
      { key: 'pos', label: 'Palette pos / hue', type: 'number', min: 0, max: 1, step: 0.01, default: 0.6 },
      { key: 'jitter', label: 'Position jitter', type: 'number', min: 0, max: 0.5, step: 0.01, default: 0.06 },
      { key: 'audioBoost', label: 'Audio spawn boost', type: 'number', min: 0, max: 4, step: 0.1, default: 2 },
    ],
  },
  init(ctx) {
    drops = [];
    lastT = ctx.t;
    pendingSpawnFrac = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const spawnRate = num(params.spawnRate, 14);
    const fallSpeed = num(params.fallSpeed, 9);
    const speedJitter = clamp01(num(params.speedJitter, 0.4));
    const trail = Math.max(0.5, Math.min(0.99, num(params.trail, 0.85)));
    const basePos = num(params.pos, 0.6);
    const posJitter = clamp01(num(params.jitter, 0.06));
    const audioBoost = num(params.audioBoost, 2);
    const paletteName = String(params.palette ?? 'arctic');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    // Decay every voxel — gives the falling trail.
    for (let i = 0; i < buffer.length; i++) buffer[i] = Math.floor(buffer[i] * trail);

    // Accumulate fractional spawn count to maintain target rate even at low fps.
    const target = spawnRate * (1 + audio.level * audioBoost);
    pendingSpawnFrac += target * dt;
    const spawnCount = Math.floor(pendingSpawnFrac);
    pendingSpawnFrac -= spawnCount;
    for (let s = 0; s < spawnCount; s++) {
      drops.push({
        x: Math.floor(Math.random() * N),
        y: Math.floor(Math.random() * N),
        z: N - 0.5 + Math.random() * 0.5,
        vz: -fallSpeed * (1 + (Math.random() - 0.5) * 2 * speedJitter),
        pos: basePos + (Math.random() - 0.5) * 2 * posJitter,
        brightness: 0.85 + Math.random() * 0.15,
      });
    }

    // Step + render. Drop the dead ones to keep the array bounded.
    const next: Drop[] = [];
    for (const d of drops) {
      d.z += d.vz * dt;
      if (d.z < -1) continue;
      const zi = Math.round(d.z);
      if (zi >= 0 && zi < N) {
        const p = ((d.pos % 1) + 1) % 1;
        const [r, g, b] = useP ? palette(p) : hsvToRgb(p, 0.85, d.brightness);
        const dr = useP ? Math.round(r * d.brightness) : r;
        const dg = useP ? Math.round(g * d.brightness) : g;
        const db = useP ? Math.round(b * d.brightness) : b;
        const i = idx(d.x, d.y, zi) * 3;
        if (i >= 0 && i + 2 < NUM_LEDS * 3) {
          buffer[i] = Math.max(buffer[i], dr);
          buffer[i + 1] = Math.max(buffer[i + 1], dg);
          buffer[i + 2] = Math.max(buffer[i + 2], db);
        }
      }
      next.push(d);
    }
    drops = next;
  },
};
