import { CUBE_N } from '../types';
import { clamp01, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

interface CometParticle {
  x: number;
  y: number;
  z: number;
  age: number;
}

// Module-level state: position, velocity, and the trail particles.
// Velocity is stored as a unit-ish direction; speed is applied each frame
// from the params + audio so it can vary smoothly without affecting the
// stored heading.
let lastT = 0;
let px = 0, py = 0, pz = 0;
let vx = 0.78, vy = 1.15, vz = 0.93;
let particles: CometParticle[] = [];

/**
 * A single bright comet bounces around the cube, leaving a fading tail.
 * The head can be a fixed RGB color or sample the head of a palette;
 * the tail either fades to black (RGB mode) or sweeps through the palette
 * along its length. On a beat, the comet speeds up briefly and decays
 * back to its base speed as the beat envelope falls — like a sprinter
 * shoved every kick.
 */
export const comet: Pattern = {
  meta: {
    id: 'comet',
    name: 'Comet',
    description: 'Bright comet bounces around the cube with a fading tail. Beats give it a temporary speed kick.',
    params: [
      { key: 'speed', label: 'Base speed (voxels/s)', type: 'number', min: 1, max: 30, step: 0.5, default: 8 },
      { key: 'tailLife', label: 'Tail life (s)', type: 'number', min: 0.1, max: 5, step: 0.05, default: 1.4 },
      { key: 'tailGamma', label: 'Tail fall-off curve', type: 'number', min: 0.3, max: 4, step: 0.1, default: 1.6 },
      { key: 'r', label: 'Head R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'g', label: 'Head G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'b', label: 'Head B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'palette', label: 'Tail palette', type: 'palette', default: 'none' },
      { key: 'beatGain', label: 'Beat → speed boost', type: 'number', min: 0, max: 8, step: 0.1, default: 2.5 },
      { key: 'beatThreshold', label: 'Beat trigger threshold', type: 'number', min: 0, max: 1, step: 0.05, default: 0.25 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    particles = [];
    // Start near a corner with a non-axis-aligned heading so the comet
    // visits the interior rather than ping-ponging on a single face.
    px = 1.5;
    py = 2.3;
    pz = 0.7;
    vx = 0.78;
    vy = 1.15;
    vz = 0.93;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const baseSpeed = num(params.speed, 8);
    const tailLife = Math.max(0.05, num(params.tailLife, 1.4));
    const tailGamma = Math.max(0.1, num(params.tailGamma, 1.6));
    const headR = num(params.r, 255);
    const headG = num(params.g, 255);
    const headB = num(params.b, 255);
    const beatGain = num(params.beatGain, 2.5);
    const beatThreshold = num(params.beatThreshold, 0.25);
    const paletteName = String(params.palette ?? 'none');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    // Speed: base × (1 + beat above threshold × gain). Using the envelope
    // value directly gives a smooth speed-up-then-fall-back as the beat
    // envelope rises and decays.
    const beat = audio.beat ?? 0;
    const beatExtra = beat > beatThreshold
      ? (beat - beatThreshold) / Math.max(0.001, 1 - beatThreshold)
      : 0;
    const speed = baseSpeed * (1 + beatExtra * beatGain);

    // Step using a unit direction so speed is in voxels/sec regardless of
    // how the velocity vector got normalized over time.
    const vLen = Math.hypot(vx, vy, vz) || 1;
    px += (vx / vLen) * speed * dt;
    py += (vy / vLen) * speed * dt;
    pz += (vz / vLen) * speed * dt;

    // Reflect off cube walls. Reflect the position around the wall and
    // flip that velocity component, like a DVD logo bouncing.
    const lo = 0;
    const hi = N - 1;
    if (px < lo) { px = lo + (lo - px); vx = Math.abs(vx); }
    else if (px > hi) { px = hi - (px - hi); vx = -Math.abs(vx); }
    if (py < lo) { py = lo + (lo - py); vy = Math.abs(vy); }
    else if (py > hi) { py = hi - (py - hi); vy = -Math.abs(vy); }
    if (pz < lo) { pz = lo + (lo - pz); vz = Math.abs(vz); }
    else if (pz > hi) { pz = hi - (pz - hi); vz = -Math.abs(vz); }

    // Spawn a particle at the new head position, expire stale ones.
    particles.push({ x: px, y: py, z: pz, age: 0 });
    const next: CometParticle[] = [];
    for (const p of particles) {
      p.age += dt;
      if (p.age <= tailLife) next.push(p);
    }
    particles = next;

    buffer.fill(0);

    // Render oldest first so the head writes last (max blend, but ordering
    // still matters when a particle revisits a recent voxel).
    for (let pi = 0; pi < particles.length; pi++) {
      const p = particles[pi];
      const lifeFrac = clamp01(1 - p.age / tailLife);
      const intensity = Math.pow(lifeFrac, tailGamma);
      if (intensity <= 0.001) continue;

      let cr: number, cg: number, cb: number;
      if (useP) {
        // Palette pos 0 = head (newest), 1 = oldest. Pick palettes whose
        // 0 end is bright and 1 end fades dark for a clean trail.
        const [r0, g0, b0] = palette(1 - lifeFrac);
        cr = r0; cg = g0; cb = b0;
      } else {
        cr = headR; cg = headG; cb = headB;
      }

      // Trilinear splat to 8 surrounding voxels so the comet moves
      // smoothly between cells instead of snapping to the integer grid.
      const x0 = Math.floor(p.x);
      const y0 = Math.floor(p.y);
      const z0 = Math.floor(p.z);
      const fx = p.x - x0;
      const fy = p.y - y0;
      const fz = p.z - z0;
      for (let ox = 0; ox <= 1; ox++) {
        const cx = x0 + ox;
        if (cx < 0 || cx >= N) continue;
        const wx = ox ? fx : 1 - fx;
        for (let oy = 0; oy <= 1; oy++) {
          const cy = y0 + oy;
          if (cy < 0 || cy >= N) continue;
          const wy = oy ? fy : 1 - fy;
          for (let oz = 0; oz <= 1; oz++) {
            const cz = z0 + oz;
            if (cz < 0 || cz >= N) continue;
            const wz = oz ? fz : 1 - fz;
            const w = wx * wy * wz * intensity;
            if (w <= 0) continue;
            const i = idx(cx, cy, cz) * 3;
            buffer[i] = Math.max(buffer[i], Math.round(cr * w));
            buffer[i + 1] = Math.max(buffer[i + 1], Math.round(cg * w));
            buffer[i + 2] = Math.max(buffer[i + 2], Math.round(cb * w));
          }
        }
      }
    }
  },
};
