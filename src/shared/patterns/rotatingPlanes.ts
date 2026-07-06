import { CUBE_N } from '../types';
import { clamp01, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * One or more planes whose normals rotate in 3D, sweeping through the cube
 * with a slow oscillating offset. Voxels within `thickness` of any plane are
 * lit, with a soft falloff. Audio level thickens the planes on transients
 * and (optionally) speeds up rotation and sweep on level + beat.
 */

// Phase accumulators in radians — needed because the angular speed varies
// with audio per frame, so we can't derive phase as t * speed.
let lastT = 0;
let rotPhase = 0;
let sweepPhase = 0;

export const rotatingPlanes: Pattern = {
  meta: {
    id: 'rotating-planes',
    name: 'Rotating Planes',
    description: 'Planes sweep through the cube while rotating. Audio thickens, accelerates, and kicks them on beats.',
    params: [
      { key: 'speed', label: 'Rotation speed', type: 'number', min: 0, max: 2, step: 0.01, default: 0.3 },
      { key: 'sweepSpeed', label: 'Sweep speed', type: 'number', min: 0, max: 2, step: 0.01, default: 0.4 },
      { key: 'thickness', label: 'Plane thickness', type: 'number', min: 0.4, max: 4, step: 0.1, default: 1 },
      { key: 'planes', label: 'Plane count', type: 'number', min: 1, max: 4, step: 1, default: 2 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'none' },
      { key: 'r', label: 'R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 80 },
      { key: 'g', label: 'G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 200 },
      { key: 'b', label: 'B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'audioGain', label: 'Audio thickness gain', type: 'number', min: 0, max: 4, step: 0.1, default: 1.5 },
      { key: 'levelSpeedGain', label: 'Level → speed', type: 'number', min: 0, max: 4, step: 0.1, default: 1.5 },
      { key: 'beatSpeedGain', label: 'Beat → speed kick', type: 'number', min: 0, max: 6, step: 0.1, default: 2.0 },
      { key: 'levelSweepGain', label: 'Level → sweep', type: 'number', min: 0, max: 4, step: 0.1, default: 1.2 },
      { key: 'beatSweepGain', label: 'Beat → sweep kick', type: 'number', min: 0, max: 6, step: 0.1, default: 1.5 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    rotPhase = 0;
    sweepPhase = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const baseSpeed = num(params.speed, 0.3);
    const baseSweepSpeed = num(params.sweepSpeed, 0.4);
    const baseThickness = Math.max(0.1, num(params.thickness, 1));
    const audioGain = num(params.audioGain, 1.5);
    const levelSpeedGain = num(params.levelSpeedGain, 1.5);
    const beatSpeedGain = num(params.beatSpeedGain, 2.0);
    const levelSweepGain = num(params.levelSweepGain, 1.2);
    const beatSweepGain = num(params.beatSweepGain, 1.5);
    const level = clamp01(audio.level);
    const beat = audio.beat ?? 0;
    const thickness = baseThickness * (1 + level * audioGain);
    const planeCount = Math.max(1, Math.floor(num(params.planes, 2)));
    const cr = num(params.r, 80);
    const cg = num(params.g, 200);
    const cb = num(params.b, 255);
    const paletteName = String(params.palette ?? 'none');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);
    const c = (N - 1) / 2;

    // Audio-modulated angular speeds. Level provides a sustained boost,
    // beat envelope adds an instantaneous kick.
    const rotRate = baseSpeed * (1 + level * levelSpeedGain) + baseSpeed * beat * beatSpeedGain;
    const sweepRate = baseSweepSpeed * (1 + level * levelSweepGain) + baseSweepSpeed * beat * beatSweepGain;
    rotPhase += rotRate * Math.PI * 2 * dt;
    sweepPhase += sweepRate * Math.PI * 0.7 * dt;

    buffer.fill(0);

    for (let p = 0; p < planeCount; p++) {
      const phase = rotPhase + (p * Math.PI) / planeCount;
      // Normal vector rotates in the xy-plane and tilts in z over time.
      const nx = Math.cos(phase);
      const ny = Math.sin(phase);
      const nz = Math.sin(phase * 0.6 + p * 0.3);
      const nLen = Math.hypot(nx, ny, nz) || 1;
      const ux = nx / nLen;
      const uy = ny / nLen;
      const uz = nz / nLen;
      // Sweep offset oscillates around 0.
      const off = Math.sin(sweepPhase + (p * Math.PI) / planeCount) * 3.5;

      for (let z = 0; z < N; z++) {
        for (let y = 0; y < N; y++) {
          for (let x = 0; x < N; x++) {
            const dx = x - c;
            const dy = y - c;
            const dz = z - c;
            const d = Math.abs(ux * dx + uy * dy + uz * dz - off);
            if (d > thickness) continue;
            const fall = 1 - d / thickness;
            // In palette mode, color depends on per-plane position (offset by p).
            let pr = cr, pg = cg, pb = cb;
            if (useP) {
              const [r0, g0, b0] = palette((p + 0.5) / planeCount);
              pr = r0; pg = g0; pb = b0;
            }
            const i = idx(x, y, z) * 3;
            buffer[i] = Math.max(buffer[i], Math.round(pr * fall));
            buffer[i + 1] = Math.max(buffer[i + 1], Math.round(pg * fall));
            buffer[i + 2] = Math.max(buffer[i + 2], Math.round(pb * fall));
          }
        }
      }
    }
  },
};
