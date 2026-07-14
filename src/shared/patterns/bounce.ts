import { CUBE_N } from '../types';
import { num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * A wireframe sphere bouncing around inside the cube (DVD-logo physics in
 * 3D), drawn as latitude rings + meridian arcs with trilinear splats. The
 * shell slowly tumbles so it reads as a solid object; the interior stays
 * dark. Beats give it a size pulse. Deliberately sparse.
 */

const N = CUBE_N;

let lastT = 0;
let cx = 0, cy = 0, cz = 0;
let vx = 0, vy = 0, vz = 0;
let spin = 0;
let pulse = 0;

function splatMax(
  buffer: Uint8Array,
  idx: (x: number, y: number, z: number) => number,
  px: number, py: number, pz: number,
  r: number, g: number, b: number,
) {
  const x0 = Math.floor(px), y0 = Math.floor(py), z0 = Math.floor(pz);
  const fx = px - x0, fy = py - y0, fz = pz - z0;
  for (let dz = 0; dz <= 1; dz++) {
    const zc = z0 + dz;
    if (zc < 0 || zc >= N) continue;
    const wz = dz === 0 ? 1 - fz : fz;
    for (let dy = 0; dy <= 1; dy++) {
      const yc = y0 + dy;
      if (yc < 0 || yc >= N) continue;
      const wy = dy === 0 ? 1 - fy : fy;
      for (let dx = 0; dx <= 1; dx++) {
        const xc = x0 + dx;
        if (xc < 0 || xc >= N) continue;
        const w = (dx === 0 ? 1 - fx : fx) * wy * wz;
        if (w <= 0.02) continue;
        const i = idx(xc, yc, zc) * 3;
        buffer[i] = Math.max(buffer[i], Math.round(r * w));
        buffer[i + 1] = Math.max(buffer[i + 1], Math.round(g * w));
        buffer[i + 2] = Math.max(buffer[i + 2], Math.round(b * w));
      }
    }
  }
}

export const bounce: Pattern = {
  meta: {
    id: 'bounce',
    name: 'Bouncing Sphere',
    description: 'A wireframe sphere bounces around the cube, slowly tumbling. Beats pulse its size.',
    params: [
      { key: 'radius', label: 'Radius (voxels)', type: 'number', min: 1.5, max: 4.5, step: 0.1, default: 2.6 },
      { key: 'speed', label: 'Bounce speed (voxels/s)', type: 'number', min: 1, max: 14, step: 0.5, default: 5 },
      { key: 'rings', label: 'Latitude rings', type: 'number', min: 1, max: 5, step: 1, default: 3 },
      { key: 'meridians', label: 'Meridians', type: 'number', min: 0, max: 6, step: 1, default: 4 },
      { key: 'spinSpeed', label: 'Tumble (rev/s)', type: 'number', min: 0, max: 1, step: 0.02, default: 0.15 },
      { key: 'beatPulse', label: 'Beat → size pulse', type: 'number', min: 0, max: 2, step: 0.05, default: 0.5 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'ocean' },
      { key: 'r', label: 'R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 120 },
      { key: 'g', label: 'G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 220 },
      { key: 'b', label: 'B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    cx = N * 0.4; cy = N * 0.55; cz = N * 0.45;
    // Non-axis-aligned heading so it visits the interior.
    vx = 0.7; vy = 1.0; vz = 0.85;
    spin = 0; pulse = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const baseR = num(params.radius, 2.6);
    const speed = num(params.speed, 5);
    const nRings = Math.max(1, Math.floor(num(params.rings, 3)));
    const nMer = Math.max(0, Math.floor(num(params.meridians, 4)));
    const spinSpeed = num(params.spinSpeed, 0.15);
    const beatPulse = num(params.beatPulse, 0.5);
    const cr = num(params.r, 120), cg = num(params.g, 220), cb = num(params.b, 255);
    const paletteName = String(params.palette ?? 'ocean');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    // Beat pulse decays; radius = base + pulse.
    pulse = Math.max(pulse * Math.exp(-4 * dt), (audio.beat ?? 0) * beatPulse);
    const R = baseR + pulse;

    // Move + bounce off walls, keeping the whole shell inside.
    const vlen = Math.hypot(vx, vy, vz) || 1;
    cx += (vx / vlen) * speed * dt;
    cy += (vy / vlen) * speed * dt;
    cz += (vz / vlen) * speed * dt;
    const lo = R, hi = N - 1 - R;
    if (cx < lo) { cx = lo + (lo - cx); vx = Math.abs(vx); } else if (cx > hi) { cx = hi - (cx - hi); vx = -Math.abs(vx); }
    if (cy < lo) { cy = lo + (lo - cy); vy = Math.abs(vy); } else if (cy > hi) { cy = hi - (cy - hi); vy = -Math.abs(vy); }
    if (cz < lo) { cz = lo + (lo - cz); vz = Math.abs(vz); } else if (cz > hi) { cz = hi - (cz - hi); vz = -Math.abs(vz); }

    spin += spinSpeed * Math.PI * 2 * dt;
    const cs = Math.cos(spin), sn = Math.sin(spin);

    // Rotate a body-frame point about the Y axis (tumble) then translate.
    const emit = (bx: number, by: number, bz: number, tCol: number) => {
      const rx = bx * cs + bz * sn;
      const rz = -bx * sn + bz * cs;
      let er = cr, eg = cg, eb = cb;
      if (useP) [er, eg, eb] = palette(tCol);
      splatMax(buffer, idx, cx + rx, cy + by, cz + rz, er, eg, eb);
    };

    // Sample density scales with radius so lines stay continuous.
    const K = Math.max(10, Math.round(R * 6));

    // Latitude rings (constant polar angle).
    for (let li = 0; li < nRings; li++) {
      const theta = ((li + 1) / (nRings + 1)) * Math.PI; // 0..pi
      const ringR = R * Math.sin(theta);
      const y = R * Math.cos(theta);
      const tCol = li / Math.max(1, nRings - 1);
      for (let k = 0; k < K; k++) {
        const phi = (k / K) * Math.PI * 2;
        emit(ringR * Math.cos(phi), y, ringR * Math.sin(phi), tCol);
      }
    }
    // Meridian arcs (pole to pole).
    for (let mi = 0; mi < nMer; mi++) {
      const phi = (mi / nMer) * Math.PI * 2;
      const cph = Math.cos(phi), sph = Math.sin(phi);
      const tCol = mi / Math.max(1, nMer);
      for (let k = 0; k <= K; k++) {
        const theta = (k / K) * Math.PI;
        const rr = R * Math.sin(theta);
        emit(rr * cph, R * Math.cos(theta), rr * sph, tCol);
      }
    }
  },
};
