import { CUBE_N } from '../types';
import { clamp01, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * Volumetric-cloud illusion: define a 3D scalar density field that drifts
 * over time, then light only the top and bottom surfaces of the cloud
 * (the wireframe-y outline) with soft edges. The interior stays dark, but
 * the edge falloff and the top/bottom shading hint at volume between the
 * lit voxels. On a strong beat the cloud spins around the vertical axis,
 * with an angular kick that damps back to rest.
 */

let lastT = 0;
let lastBeat = 0;
let spin = 0;
let spinVel = 0;

// Cheap, deterministic 3D noise: sum of phase-offset sines at a few
// spatial frequencies so we get lumps within a 10-voxel cube rather than
// one slow blob that fills it.
function densityField(x: number, y: number, z: number, t: number): number {
  return (
    Math.sin(x * 1.7 + y * 0.9 + t * 0.6) * 0.55 +
    Math.sin(z * 1.3 - y * 1.1 + t * 0.4) * 0.55 +
    Math.sin((x + z) * 0.9 + y * 0.7 + t * 0.3) * 0.45 +
    Math.sin((x - z) * 1.1 + y * 1.4 - t * 0.35) * 0.35 +
    Math.sin(x * 2.6 + z * 2.2 + t * 0.5) * 0.25
  ) / 2.15;
}

export const cloud: Pattern = {
  meta: {
    id: 'cloud',
    name: 'Cloud',
    description: 'Volumetric cloud — only the top and bottom surfaces are lit, with soft gradient edges. Spins on beat.',
    params: [
      { key: 'threshold', label: 'Density threshold', type: 'number', min: -0.4, max: 0.6, step: 0.02, default: 0 },
      { key: 'scale', label: 'Cloud scale', type: 'number', min: 0.1, max: 1.2, step: 0.02, default: 0.5 },
      { key: 'driftSpeed', label: 'Drift speed', type: 'number', min: 0, max: 1, step: 0.02, default: 0.25 },
      { key: 'edge', label: 'Edge softness', type: 'number', min: 0.05, max: 0.6, step: 0.01, default: 0.18 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'none' },
      { key: 'r', label: 'R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 220 },
      { key: 'g', label: 'G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 235 },
      { key: 'b', label: 'B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'topTint', label: 'Top→bottom tint shift', type: 'number', min: 0, max: 1, step: 0.05, default: 0.4 },
      { key: 'spinKick', label: 'Beat spin kick (rad/s)', type: 'number', min: 0, max: 6, step: 0.1, default: 1.4 },
      { key: 'spinDamp', label: 'Spin damping', type: 'number', min: 0.2, max: 6, step: 0.1, default: 1.2 },
      { key: 'beatThreshold', label: 'Beat trigger threshold', type: 'number', min: 0.1, max: 1, step: 0.05, default: 0.5 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    lastBeat = 0;
    spin = 0;
    spinVel = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const threshold = num(params.threshold, 0);
    const scale = Math.max(0.01, num(params.scale, 0.5));
    const driftSpeed = num(params.driftSpeed, 0.25);
    const edge = Math.max(0.02, num(params.edge, 0.18));
    const baseR = num(params.r, 220);
    const baseG = num(params.g, 235);
    const baseB = num(params.b, 255);
    const topTint = clamp01(num(params.topTint, 0.4));
    const spinKick = num(params.spinKick, 1.4);
    const spinDamp = Math.max(0.05, num(params.spinDamp, 1.2));
    const beatThreshold = num(params.beatThreshold, 0.5);
    const paletteName = String(params.palette ?? 'none');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    // Spin kick on rising-edge of beat envelope.
    const beat = audio.beat ?? 0;
    if (beat > beatThreshold && lastBeat <= beatThreshold) {
      const dir = ((spinVel >= 0 ? 1 : -1) * (Math.random() < 0.85 ? 1 : -1));
      spinVel += spinKick * dir * (0.7 + 0.6 * (audio.level ?? 0));
    }
    lastBeat = beat;
    // Damp spin and integrate.
    spinVel *= Math.exp(-spinDamp * dt);
    spin += spinVel * dt;

    buffer.fill(0);

    const c = (N - 1) / 2;
    const cosA = Math.cos(spin);
    const sinA = Math.sin(spin);
    const drift = t * driftSpeed;

    // For each (x,z) column, compute density at every y first so we can
    // detect top/bottom surfaces (a cell is on the top surface if it's
    // above threshold and the cell above is below it; same idea for bottom).
    const dens = new Float32Array(N);

    for (let x = 0; x < N; x++) {
      for (let z = 0; z < N; z++) {
        // Rotate the (x,z) sample point around vertical axis so the cloud
        // "spins" without re-mapping the LED grid itself.
        const rx = (x - c) * cosA - (z - c) * sinA;
        const rz = (x - c) * sinA + (z - c) * cosA;

        for (let y = 0; y < N; y++) {
          dens[y] = densityField(rx * scale, (y - c) * scale, rz * scale, drift);
        }

        for (let y = 0; y < N; y++) {
          const d = dens[y];
          // Treat out-of-bounds as "still inside the cloud" (a large value
          // above threshold). Otherwise the cube's top/bottom face would
          // be classified as a surface whenever the cloud touched it,
          // lighting up the whole plane of the box rather than the plane
          // of the cloud's actual silhouette.
          const dAbove = y < N - 1 ? dens[y + 1] : 1e3;
          const dBelow = y > 0 ? dens[y - 1] : 1e3;

          // "How much above threshold this cell is" (0..1 over `edge`).
          const here = clamp01((d - threshold) / edge);
          if (here <= 0) continue;
          // Surface-ness above/below: how much the neighbor sits *below*
          // threshold. Multiplying with `here` gives a soft top/bottom strip.
          const topness = here * clamp01((threshold - dAbove) / edge);
          const bottomness = here * clamp01((threshold - dBelow) / edge);
          let intensity = topness + bottomness;
          if (intensity <= 0.01) continue;
          intensity = clamp01(intensity);

          // Tint shift from top→bottom: top a bit cooler/lighter, bottom
          // shaded. With a palette, sample top→bottom across the gradient.
          let cr: number, cg: number, cb: number;
          if (useP) {
            const yPos = y / (N - 1);
            const [pr, pg, pb] = palette(yPos);
            cr = pr; cg = pg; cb = pb;
          } else {
            // Top of cloud (high y) keeps base; bottom darkens by topTint.
            const shade = 1 - topTint * (1 - y / (N - 1));
            cr = baseR * shade;
            cg = baseG * shade;
            cb = baseB * shade;
          }

          const i = idx(x, y, z) * 3;
          buffer[i] = Math.round(cr * intensity);
          buffer[i + 1] = Math.round(cg * intensity);
          buffer[i + 2] = Math.round(cb * intensity);
        }
      }
    }
  },
};
