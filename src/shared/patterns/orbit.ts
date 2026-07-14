import { CUBE_N } from '../types';
import { num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * Atom-style orbiting particles: a handful of bright points circle the cube's
 * center, each in its own tilted plane, trailing a short fading comet-tail.
 * Extremely sparse — a few dozen lit voxels against black — which is where
 * the cube reads as a 3D object rather than a light panel. Beats speed the
 * orbits and flare the heads.
 */

const N = CUBE_N;
const MAX_PARTICLES = 6;
const TRAIL = 14;

interface Orbiter {
  // Orthonormal basis for the orbital plane.
  ux: number; uy: number; uz: number;
  vx: number; vy: number; vz: number;
  radius: number;
  phase: number;
  freq: number;
  tCol: number;
  trail: [number, number, number][];
}

let lastT = 0;
let orbiters: Orbiter[] = [];
let spin = 0;

// Deterministic tilted planes so it looks designed, not random.
function makeOrbiters(nn: number, radius: number): Orbiter[] {
  const out: Orbiter[] = [];
  for (let i = 0; i < nn; i++) {
    // Tilt each plane by a golden-ratio-spaced normal direction.
    const a = i * 2.399963; // golden angle
    const b = i * 1.107 + 0.4;
    const nx = Math.cos(a) * Math.sin(b);
    const ny = Math.sin(a) * Math.sin(b);
    const nz = Math.cos(b);
    // Build an orthonormal (u,v) spanning the plane with that normal.
    let ux = -Math.sin(a), uy = Math.cos(a), uz = 0;
    const ul = Math.hypot(ux, uy, uz) || 1;
    ux /= ul; uy /= ul; uz /= ul;
    // v = n × u
    const vx = ny * uz - nz * uy;
    const vy = nz * ux - nx * uz;
    const vz = nx * uy - ny * ux;
    out.push({
      ux, uy, uz, vx, vy, vz,
      radius,
      phase: i * 1.7,
      freq: 0.5 + 0.12 * i, // slightly different rates so they drift apart
      tCol: i / Math.max(1, nn),
      trail: [],
    });
  }
  return out;
}

export const orbit: Pattern = {
  meta: {
    id: 'orbit',
    name: 'Orbiting Particles',
    description: 'Bright points circle the center in tilted planes with comet trails. Beats speed and flare them.',
    params: [
      { key: 'count', label: 'Particle count', type: 'number', min: 1, max: 6, step: 1, default: 4 },
      { key: 'radius', label: 'Orbit radius (voxels)', type: 'number', min: 1.5, max: 4.5, step: 0.1, default: 3.4 },
      { key: 'speed', label: 'Orbit speed (rev/s)', type: 'number', min: 0.05, max: 2, step: 0.05, default: 0.4 },
      { key: 'trail', label: 'Trail length', type: 'number', min: 0, max: 14, step: 1, default: 10 },
      { key: 'precess', label: 'Precession (rev/s)', type: 'number', min: 0, max: 0.5, step: 0.01, default: 0.05 },
      { key: 'beatSpeed', label: 'Beat → speed', type: 'number', min: 0, max: 4, step: 0.1, default: 1.5 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'spectrum' },
      { key: 'headBright', label: 'Head brightness', type: 'number', min: 0.3, max: 1, step: 0.05, default: 1 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    spin = 0;
    orbiters = makeOrbiters(4, 3.4);
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const count = Math.max(1, Math.min(MAX_PARTICLES, Math.floor(num(params.count, 4))));
    const radius = num(params.radius, 3.4);
    const speed = num(params.speed, 0.4);
    const trailLen = Math.max(0, Math.min(TRAIL, Math.floor(num(params.trail, 10))));
    const precess = num(params.precess, 0.05);
    const beatSpeed = num(params.beatSpeed, 1.5);
    const headBright = num(params.headBright, 1);
    const paletteName = String(params.palette ?? 'spectrum');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    if (orbiters.length !== count) orbiters = makeOrbiters(count, radius);
    for (const o of orbiters) o.radius = radius;

    const c = (N - 1) / 2;
    const boost = 1 + (audio.beat ?? 0) * beatSpeed;
    // Global slow precession rotates all planes about Y so orbits evolve.
    spin += precess * Math.PI * 2 * dt;
    const cs = Math.cos(spin), sn = Math.sin(spin);

    const splat = (px: number, py: number, pz: number, r: number, g: number, b: number) => {
      const x0 = Math.floor(px), y0 = Math.floor(py), z0 = Math.floor(pz);
      const fx = px - x0, fy = py - y0, fz = pz - z0;
      for (let dz = 0; dz <= 1; dz++) {
        const zc = z0 + dz; if (zc < 0 || zc >= N) continue;
        const wz = dz === 0 ? 1 - fz : fz;
        for (let dy = 0; dy <= 1; dy++) {
          const yc = y0 + dy; if (yc < 0 || yc >= N) continue;
          const wy = dy === 0 ? 1 - fy : fy;
          for (let dx = 0; dx <= 1; dx++) {
            const xc = x0 + dx; if (xc < 0 || xc >= N) continue;
            const w = (dx === 0 ? 1 - fx : fx) * wy * wz;
            if (w <= 0.02) continue;
            const i = idx(xc, yc, zc) * 3;
            buffer[i] = Math.max(buffer[i], Math.round(r * w));
            buffer[i + 1] = Math.max(buffer[i + 1], Math.round(g * w));
            buffer[i + 2] = Math.max(buffer[i + 2], Math.round(b * w));
          }
        }
      }
    };

    for (const o of orbiters) {
      o.phase += o.freq * speed * boost * Math.PI * 2 * dt;
      const ca = Math.cos(o.phase), sa = Math.sin(o.phase);
      // Point on the orbit plane, then precess about Y.
      let px = o.radius * (ca * o.ux + sa * o.vx);
      let py = o.radius * (ca * o.uy + sa * o.vy);
      let pz = o.radius * (ca * o.uz + sa * o.vz);
      const rx = px * cs + pz * sn;
      const rz = -px * sn + pz * cs;
      px = c + rx; py = c + py; pz = c + rz;

      o.trail.unshift([px, py, pz]);
      if (o.trail.length > trailLen + 1) o.trail.length = trailLen + 1;

      let hr = 255, hg = 255, hb = 255;
      if (useP) [hr, hg, hb] = palette(o.tCol);
      // Trail: oldest first so head writes last.
      for (let s = o.trail.length - 1; s >= 0; s--) {
        const [tx, ty, tz] = o.trail[s];
        const k = s === 0 ? headBright : headBright * (1 - s / (trailLen + 1)) * 0.8;
        if (k <= 0.02) continue;
        splat(tx, ty, tz, hr * k, hg * k, hb * k);
      }
    }
  },
};
