import { CUBE_N } from '../types';
import { num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * A wireframe cube tumbling inside the cube — 12 edges drawn with trilinear
 * splats so lines stay smooth between voxels. Deliberately sparse: only the
 * edges (and slightly brighter corners) light up, which reads unmistakably
 * as a 3D object floating in the volume. Beats kick the spin.
 */

const N = CUBE_N;

let lastT = 0;
let angA = 0;
let angB = 0;
let boost = 0;

const CORNERS: [number, number, number][] = [
  [-1, -1, -1], [1, -1, -1], [1, 1, -1], [-1, 1, -1],
  [-1, -1, 1], [1, -1, 1], [1, 1, 1], [-1, 1, 1],
];
const EDGES: [number, number][] = [
  [0, 1], [1, 2], [2, 3], [3, 0],  // bottom face
  [4, 5], [5, 6], [6, 7], [7, 4],  // top face
  [0, 4], [1, 5], [2, 6], [3, 7],  // verticals
];

function splatMax(
  buffer: Uint8Array,
  idx: (x: number, y: number, z: number) => number,
  px: number, py: number, pz: number,
  r: number, g: number, b: number,
) {
  const x0 = Math.floor(px), y0 = Math.floor(py), z0 = Math.floor(pz);
  const fx = px - x0, fy = py - y0, fz = pz - z0;
  for (let dz = 0; dz <= 1; dz++) {
    const cz = z0 + dz;
    if (cz < 0 || cz >= N) continue;
    const wz = dz === 0 ? 1 - fz : fz;
    for (let dy = 0; dy <= 1; dy++) {
      const cy = y0 + dy;
      if (cy < 0 || cy >= N) continue;
      const wy = dy === 0 ? 1 - fy : fy;
      for (let dx = 0; dx <= 1; dx++) {
        const cx = x0 + dx;
        if (cx < 0 || cx >= N) continue;
        const w = (dx === 0 ? 1 - fx : fx) * wy * wz;
        if (w <= 0.02) continue;
        const i = idx(cx, cy, cz) * 3;
        buffer[i] = Math.max(buffer[i], Math.round(r * w));
        buffer[i + 1] = Math.max(buffer[i + 1], Math.round(g * w));
        buffer[i + 2] = Math.max(buffer[i + 2], Math.round(b * w));
      }
    }
  }
}

export const spinCube: Pattern = {
  meta: {
    id: 'spin-cube',
    name: 'Spinning Cube',
    description: 'A wireframe cube tumbles inside the volume; beats kick the spin.',
    params: [
      { key: 'size', label: 'Cube size (voxels)', type: 'number', min: 2, max: 9, step: 0.25, default: 5.5 },
      { key: 'speedA', label: 'Tumble speed A (rev/s)', type: 'number', min: 0, max: 0.6, step: 0.01, default: 0.09 },
      { key: 'speedB', label: 'Tumble speed B (rev/s)', type: 'number', min: 0, max: 0.6, step: 0.01, default: 0.06 },
      { key: 'breathe', label: 'Size breathing', type: 'number', min: 0, max: 0.5, step: 0.02, default: 0.12 },
      { key: 'beatKick', label: 'Beat → spin kick', type: 'number', min: 0, max: 6, step: 0.1, default: 2.0 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'none' },
      { key: 'r', label: 'R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 240 },
      { key: 'g', label: 'G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 240 },
      { key: 'b', label: 'B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'cornerBoost', label: 'Corner brightness', type: 'number', min: 1, max: 2, step: 0.05, default: 1.35 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    angA = 0.5;
    angB = 0.2;
    boost = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const baseSize = num(params.size, 5.5);
    const speedA = num(params.speedA, 0.09);
    const speedB = num(params.speedB, 0.06);
    const breathe = num(params.breathe, 0.12);
    const beatKick = num(params.beatKick, 2.0);
    const cr = num(params.r, 240);
    const cg = num(params.g, 240);
    const cb = num(params.b, 255);
    const cornerBoost = num(params.cornerBoost, 1.35);
    const paletteName = String(params.palette ?? 'none');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    const beat = audio.beat ?? 0;
    boost = Math.max(boost * Math.exp(-2 * dt), beat * beatKick);
    angA += speedA * (1 + boost) * Math.PI * 2 * dt;
    angB += speedB * (1 + boost * 0.6) * Math.PI * 2 * dt;

    const half = (baseSize * (1 + breathe * Math.sin(t * 0.9))) / 2;
    const c = (N - 1) / 2;
    const cosA = Math.cos(angA), sinA = Math.sin(angA);
    const cosB = Math.cos(angB), sinB = Math.sin(angB);

    // Rotate corners: Y-axis by angB, then X-axis by angA.
    const pts: [number, number, number][] = CORNERS.map(([ux, uy, uz]) => {
      let x = ux * half, y = uy * half, z = uz * half;
      const x1 = x * cosB + z * sinB;
      const z1 = -x * sinB + z * cosB;
      const y2 = y * cosA - z1 * sinA;
      const z2 = y * sinA + z1 * cosA;
      return [c + x1, c + y2, c + z2];
    });

    for (let e = 0; e < EDGES.length; e++) {
      const [a, b] = EDGES[e];
      const [ax, ay, az] = pts[a];
      const [bx, by, bz] = pts[b];
      const steps = Math.max(2, Math.ceil(Math.hypot(bx - ax, by - ay, bz - az) * 2.5));
      for (let s = 0; s <= steps; s++) {
        const f = s / steps;
        let er = cr, eg = cg, eb = cb;
        if (useP) {
          // Each edge gets its own palette stop so the frame reads colorful.
          [er, eg, eb] = palette(e / (EDGES.length - 1));
        }
        splatMax(buffer, idx, ax + (bx - ax) * f, ay + (by - ay) * f, az + (bz - az) * f, er, eg, eb);
      }
    }
    // Corners a touch brighter — sells the vertices.
    for (const [px, py, pz] of pts) {
      splatMax(buffer, idx, px, py, pz,
               Math.min(255, cr * cornerBoost), Math.min(255, cg * cornerBoost),
               Math.min(255, cb * cornerBoost));
    }
  },
};
