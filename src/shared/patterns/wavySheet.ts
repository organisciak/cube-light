import { CUBE_N } from '../types';
import { clamp01, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

// Phase accumulator for the in-plane rotation. Stored at module level so
// the rotation rate can vary (e.g. when audio-reactive) without phase
// jumps from t * speed style derivations.
let lastT = 0;
let rotPhase = 0;

/**
 * A horizontal sheet of pixels that gently waves up/down.
 * Multi-band audio mapping:
 *   - level     → global amplitude scale
 *   - bass      → continuous radial spike
 *   - mid       → modulates the y-axis wave amplitude
 *   - treble    → small high-frequency surface noise
 *   - beat      → instantaneous amplitude kick + central swell
 */
export const wavySheet: Pattern = {
  meta: {
    id: 'wavy-sheet',
    name: 'Wavy Sheet',
    description: 'Mid-plane mesh; level/bass/mid/treble/beat all drive different aspects.',
    params: [
      { key: 'axis', label: 'Wave-height axis', type: 'enum', options: ['x', 'y', 'z'], default: 'z' },
      { key: 'baseZ', label: 'Base layer', type: 'number', min: 0, max: CUBE_N - 1, step: 1, default: 4 },
      { key: 'amp', label: 'Wave amplitude', type: 'number', min: 0, max: 4, step: 0.1, default: 1.2 },
      { key: 'speed', label: 'Wave speed', type: 'number', min: 0, max: 3, step: 0.05, default: 0.6 },
      { key: 'wavelength', label: 'Wavelength (pixels)', type: 'number', min: 2, max: 30, step: 0.5, default: 8 },
      { key: 'thickness', label: 'Sheet thickness', type: 'number', min: 0.4, max: 4, step: 0.1, default: 1.0 },
      { key: 'levelGain', label: 'Audio level → amp', type: 'number', min: 0, max: 6, step: 0.1, default: 2.0 },
      { key: 'bassGain', label: 'Bass → spike', type: 'number', min: 0, max: 6, step: 0.1, default: 2.5 },
      { key: 'midGain', label: 'Mid → wave mod', type: 'number', min: 0, max: 3, step: 0.05, default: 0.5 },
      { key: 'trebleGain', label: 'Treble → surface noise', type: 'number', min: 0, max: 3, step: 0.05, default: 0.6 },
      { key: 'beatGain', label: 'Beat → amp kick', type: 'number', min: 0, max: 4, step: 0.1, default: 1.5 },
      { key: 'rotateSpeed', label: 'Rotate speed (rev/s)', type: 'number', min: 0, max: 0.5, step: 0.005, default: 0.02 },
      { key: 'rotateAudio', label: 'Audio-reactive rotation', type: 'bool', default: false },
      { key: 'rotateLevelGain', label: 'Level → rotate boost', type: 'number', min: 0, max: 4, step: 0.1, default: 1.5 },
      { key: 'rotateBeatGain', label: 'Beat → rotate kick', type: 'number', min: 0, max: 6, step: 0.1, default: 2.0 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'none' },
      { key: 'r', label: 'R (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 220 },
      { key: 'g', label: 'G (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 220 },
      { key: 'b', label: 'B (RGB mode)', type: 'number', min: 0, max: 255, step: 1, default: 255 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    rotPhase = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;
    buffer.fill(0);

    const N = CUBE_N;
    const axis = (String(params.axis ?? 'z') as 'x' | 'y' | 'z');
    const baseZ = num(params.baseZ, 4);
    const amp = num(params.amp, 1.2);
    const speed = num(params.speed, 0.6);
    const wl = Math.max(0.5, num(params.wavelength, 8));
    const thickness = Math.max(0.1, num(params.thickness, 1));
    const levelGain = num(params.levelGain, 2.0);
    const bassGain = num(params.bassGain, 2.5);
    const midGain = num(params.midGain, 0.5);
    const trebleGain = num(params.trebleGain, 0.6);
    const beatGain = num(params.beatGain, 1.5);
    const rotateSpeed = num(params.rotateSpeed, 0.02);
    const rotateAudio = Boolean(params.rotateAudio ?? false);
    const rotateLevelGain = num(params.rotateLevelGain, 1.5);
    const rotateBeatGain = num(params.rotateBeatGain, 2.0);
    const cr = num(params.r, 220);
    const cg = num(params.g, 220);
    const cb = num(params.b, 255);
    const paletteName = String(params.palette ?? 'none');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    const k = (Math.PI * 2) / wl;
    const wt = t * speed * Math.PI * 2;
    const bass = audio.bands[0] ?? 0;
    const mid = audio.bands[Math.floor(audio.bands.length / 2)] ?? 0;
    const treble = audio.bands[audio.bands.length - 1] ?? 0;
    const beat = audio.beat ?? 0;
    const audioAmp = amp * (1 + audio.level * levelGain + beat * beatGain);

    // Integrate the rotation phase so the rate can vary smoothly when
    // audio-reactive without phase jumps. In audio mode the base rate is
    // boosted by level and kicked by the beat envelope; otherwise it's
    // just a constant slow drift.
    const rotRate = rotateAudio
      ? rotateSpeed * (1 + clamp01(audio.level) * rotateLevelGain) + rotateSpeed * beat * rotateBeatGain
      : rotateSpeed;
    rotPhase += rotRate * Math.PI * 2 * dt;
    const cosR = Math.cos(rotPhase);
    const sinR = Math.sin(rotPhase);

    // The two "plane axes" sweep across the sheet; the "height axis" gets
    // displaced by the surface formula. Mapping is a permutation of x/y/z.
    const setIdx = (a: number, b: number, h: number): number => {
      if (axis === 'z') return idx(a, b, h);
      if (axis === 'y') return idx(a, h, b);
      return idx(h, a, b);
    };

    for (let a = 0; a < N; a++) {
      for (let b = 0; b < N; b++) {
        const da0 = a - (N - 1) / 2;
        const db0 = b - (N - 1) / 2;
        // Rotate the in-plane sample point so the wavefronts spin around
        // the sheet's center while the LED grid itself stays put.
        const da = da0 * cosR - db0 * sinR;
        const db = da0 * sinR + db0 * cosR;
        const r = Math.hypot(da, db);
        const surface =
          baseZ +
          audioAmp *
            (Math.sin(k * da + wt) * 0.55 +
              Math.sin(k * db - wt * 0.8) * (0.55 + midGain * mid) +
              bass * bassGain * Math.cos(k * r * 0.7 - wt * 1.5) +
              // Beat causes a brief radial swell from the center.
              beat * 2.0 * Math.exp(-r * 0.3) +
              // Treble adds high-frequency surface jitter.
              trebleGain * treble * Math.sin(k * 4 * (da + db) + wt * 5));

        const hMin = Math.max(0, Math.floor(surface - thickness));
        const hMax = Math.min(N - 1, Math.ceil(surface + thickness));
        for (let h = hMin; h <= hMax; h++) {
          const d = Math.abs(h - surface);
          if (d > thickness) continue;
          const fall = 1 - d / thickness;
          let pr = cr, pg = cg, pb = cb;
          if (useP) {
            // Palette samples along the height axis so layers gradient up.
            const [r0, g0, b0] = palette(h / (N - 1));
            pr = r0; pg = g0; pb = b0;
          }
          const i = setIdx(a, b, h) * 3;
          buffer[i] = Math.max(buffer[i], Math.round(pr * fall));
          buffer[i + 1] = Math.max(buffer[i + 1], Math.round(pg * fall));
          buffer[i + 2] = Math.max(buffer[i + 2], Math.round(pb * fall));
        }
      }
    }
  },
};
