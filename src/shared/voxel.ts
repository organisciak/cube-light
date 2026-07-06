import { CUBE_N } from './types';

/**
 * Voxel primitives for patterns. Each primitive writes into a flat
 * Uint8Array buffer using the supplied (x,y,z)→ledIdx map. Color blending
 * defaults to "max" (brighter wins) so overlapping shapes don't dim each
 * other. Patterns can opt in to "add" or "replace" modes.
 */

export type IdxFn = (x: number, y: number, z: number) => number;

export type BlendMode = 'max' | 'add' | 'replace';

const N = CUBE_N;

function blendOnce(buf: Uint8Array, i: number, r: number, g: number, b: number, mode: BlendMode) {
  if (mode === 'max') {
    if (r > buf[i]) buf[i] = r;
    if (g > buf[i + 1]) buf[i + 1] = g;
    if (b > buf[i + 2]) buf[i + 2] = b;
  } else if (mode === 'add') {
    buf[i] = Math.min(255, buf[i] + r);
    buf[i + 1] = Math.min(255, buf[i + 1] + g);
    buf[i + 2] = Math.min(255, buf[i + 2] + b);
  } else {
    buf[i] = r;
    buf[i + 1] = g;
    buf[i + 2] = b;
  }
}

/**
 * Spherical shell of `thickness` around (cx, cy, cz) with linear falloff.
 * If `thickness` is 0, draws a single voxel point.
 */
export function drawSphere(
  buf: Uint8Array,
  idx: IdxFn,
  cx: number, cy: number, cz: number,
  radius: number, thickness: number,
  color: [number, number, number],
  mode: BlendMode = 'max',
) {
  const [cr, cg, cb] = color;
  const lo = Math.max(0, Math.floor(cx - radius - thickness));
  const hi = Math.min(N - 1, Math.ceil(cx + radius + thickness));
  const t = Math.max(0.001, thickness);
  for (let z = 0; z < N; z++) {
    for (let y = 0; y < N; y++) {
      for (let x = lo; x <= hi; x++) {
        const dx = x - cx, dy = y - cy, dz = z - cz;
        const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
        const d = Math.abs(dist - radius);
        if (d > t) continue;
        const f = 1 - d / t;
        const i = idx(x, y, z) * 3;
        blendOnce(buf, i, Math.round(cr * f), Math.round(cg * f), Math.round(cb * f), mode);
      }
    }
  }
}

/**
 * Solid plane defined by unit normal (ux, uy, uz) and signed offset.
 * Voxels within `thickness` of the plane (centered on cube center) are lit.
 */
export function drawPlane(
  buf: Uint8Array,
  idx: IdxFn,
  ux: number, uy: number, uz: number,
  offset: number, thickness: number,
  color: [number, number, number],
  mode: BlendMode = 'max',
) {
  const c = (N - 1) / 2;
  const [cr, cg, cb] = color;
  const t = Math.max(0.001, thickness);
  for (let z = 0; z < N; z++) {
    for (let y = 0; y < N; y++) {
      for (let x = 0; x < N; x++) {
        const dx = x - c, dy = y - c, dz = z - c;
        const d = Math.abs(ux * dx + uy * dy + uz * dz - offset);
        if (d > t) continue;
        const f = 1 - d / t;
        const i = idx(x, y, z) * 3;
        blendOnce(buf, i, Math.round(cr * f), Math.round(cg * f), Math.round(cb * f), mode);
      }
    }
  }
}

/**
 * Single voxel point with optional brightness scale. Bounds-checked.
 */
export function drawPoint(
  buf: Uint8Array,
  idx: IdxFn,
  x: number, y: number, z: number,
  color: [number, number, number],
  mode: BlendMode = 'max',
) {
  if (x < 0 || x >= N || y < 0 || y >= N || z < 0 || z >= N) return;
  const i = idx(x | 0, y | 0, z | 0) * 3;
  blendOnce(buf, i, color[0], color[1], color[2], mode);
}

/**
 * 3D Bresenham-style voxel line from (x0,y0,z0) to (x1,y1,z1).
 */
export function drawVoxelLine(
  buf: Uint8Array,
  idx: IdxFn,
  x0: number, y0: number, z0: number,
  x1: number, y1: number, z1: number,
  color: [number, number, number],
  mode: BlendMode = 'max',
) {
  let x = x0 | 0, y = y0 | 0, z = z0 | 0;
  const dx = Math.abs((x1 | 0) - x);
  const dy = Math.abs((y1 | 0) - y);
  const dz = Math.abs((z1 | 0) - z);
  const sx = (x1 | 0) > x ? 1 : -1;
  const sy = (y1 | 0) > y ? 1 : -1;
  const sz = (z1 | 0) > z ? 1 : -1;
  const dm = Math.max(dx, dy, dz);
  let xErr = dm / 2, yErr = dm / 2, zErr = dm / 2;
  for (let i = 0; i <= dm; i++) {
    drawPoint(buf, idx, x, y, z, color, mode);
    xErr -= dx;
    if (xErr < 0) { xErr += dm; x += sx; }
    yErr -= dy;
    if (yErr < 0) { yErr += dm; y += sy; }
    zErr -= dz;
    if (zErr < 0) { zErr += dm; z += sz; }
  }
}

/**
 * Implicit-surface "sheet": evaluates surface(x, y) → z, lights voxels
 * within `thickness` of that z. Useful for displacement-driven meshes.
 */
export function drawSheet(
  buf: Uint8Array,
  idx: IdxFn,
  surface: (x: number, y: number) => number,
  thickness: number,
  color: (x: number, y: number, z: number, distNorm: number) => [number, number, number],
  mode: BlendMode = 'max',
) {
  const t = Math.max(0.001, thickness);
  for (let x = 0; x < N; x++) {
    for (let y = 0; y < N; y++) {
      const sZ = surface(x, y);
      const zMin = Math.max(0, Math.floor(sZ - t));
      const zMax = Math.min(N - 1, Math.ceil(sZ + t));
      for (let z = zMin; z <= zMax; z++) {
        const d = Math.abs(z - sZ);
        if (d > t) continue;
        const f = 1 - d / t;
        const [r, g, b] = color(x, y, z, f);
        const i = idx(x, y, z) * 3;
        blendOnce(buf, i, r, g, b, mode);
      }
    }
  }
}

/** Fade every voxel by `factor` (0..1). 0.85 means 15% decay per call. */
export function decayBuffer(buf: Uint8Array, factor: number) {
  const f = Math.max(0, Math.min(1, factor));
  for (let i = 0; i < buf.length; i++) buf[i] = Math.floor(buf[i] * f);
}
