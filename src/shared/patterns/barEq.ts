import { AUDIO_BANDS, CUBE_N } from '../types';
import { clamp01, hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * 3D bar equalizer: the floor is divided into a 5×5 grid of 2×2-footprint
 * columns that throb upward with the music. Each bar's frequency position
 * runs along the (x+y) diagonal, so the spectrum reads as a classic
 * equalizer from two adjacent faces of the cube at once (lows in one corner,
 * highs in the opposite corner).
 *
 * Same asymmetric attack/release smoothing as spectrumDiscs so bars snap up
 * fast and fall gracefully. Beat adds a global kick on top.
 */

const N = CUBE_N;
const BARS = N / 2; // 5 bars per side, 2×2 cells each

let smoothed: number[] = new Array(BARS * BARS).fill(0);

export const barEq: Pattern = {
  meta: {
    id: 'bar-eq',
    name: 'Bar Equalizer',
    description: '5×5 grid of 2×2 columns throbbing to the spectrum; lows to highs run corner-to-corner.',
    params: [
      { key: 'gain', label: 'Audio gain', type: 'number', min: 0.1, max: 4, step: 0.1, default: 1.5 },
      { key: 'gamma', label: 'Response curve', type: 'number', min: 0.3, max: 3, step: 0.05, default: 0.8 },
      { key: 'attack', label: 'Attack (s)', type: 'number', min: 0, max: 0.5, step: 0.01, default: 0.03 },
      { key: 'release', label: 'Release (s)', type: 'number', min: 0.02, max: 1.5, step: 0.02, default: 0.3 },
      { key: 'beatBoost', label: 'Beat boost', type: 'number', min: 0, max: 1, step: 0.05, default: 0.2 },
      { key: 'baseHeight', label: 'Idle bar height', type: 'number', min: 0, max: 3, step: 0.1, default: 0.6 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'spectrum' },
      { key: 'colorBy', label: 'Color by', type: 'enum', options: ['bar', 'height', 'band'], default: 'bar' },
      { key: 'sat', label: 'Saturation (HSV mode)', type: 'number', min: 0, max: 1, step: 0.05, default: 0.9 },
    ],
  },
  init(ctx) {
    smoothed = new Array(BARS * BARS).fill(0);
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, params, audio, dt } = ctx;
    const gain = num(params.gain, 1.5);
    const gamma = Math.max(0.1, num(params.gamma, 0.8));
    const attack = Math.max(0, num(params.attack, 0.03));
    const release = Math.max(0.01, num(params.release, 0.3));
    const beatBoost = clamp01(num(params.beatBoost, 0.2));
    const baseHeight = num(params.baseHeight, 0.6);
    const sat = num(params.sat, 0.9);
    const colorBy = String(params.colorBy ?? 'bar');
    const paletteName = String(params.palette ?? 'spectrum');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    const beat = audio.beat ?? 0;

    buffer.fill(0);

    for (let by = 0; by < BARS; by++) {
      for (let bx = 0; bx < BARS; bx++) {
        const bi = by * BARS + bx;
        // Frequency position along the diagonal: (0,0) = lows, (4,4) = highs.
        const bandPos = (bx + by) / (2 * (BARS - 1));
        const bf = bandPos * (AUDIO_BANDS - 1);
        const b0 = Math.floor(bf);
        const b1 = Math.min(AUDIO_BANDS - 1, b0 + 1);
        const f = bf - b0;
        const target = clamp01(((audio.bands[b0] ?? 0) * (1 - f) + (audio.bands[b1] ?? 0) * f) * gain);
        const tau = target > smoothed[bi] ? attack : release;
        const k = tau <= 0 ? 1 : 1 - Math.exp(-dt / tau);
        smoothed[bi] += (target - smoothed[bi]) * k;

        const energy = clamp01(smoothed[bi] + beatBoost * beat);
        const shaped = Math.pow(energy, gamma);
        // Bar height in voxels; idle bars keep a dim base so the floor reads.
        const height = baseHeight + shaped * (N - baseHeight);

        // 'bar' mode: one solid color per column, scrambled across the
        // palette so adjacent bars contrast instead of blending.
        const barT = ((bi * 7) % (BARS * BARS)) / (BARS * BARS - 1);
        for (let z = 0; z < N; z++) {
          // Antialiased top: full below, fractional coverage at the crest.
          const cover = clamp01(height - z);
          if (cover <= 0.02) break;
          const t01 = colorBy === 'band' ? bandPos : colorBy === 'bar' ? barT : z / (N - 1);
          let r: number, g: number, b: number;
          if (useP) {
            [r, g, b] = palette(t01);
          } else {
            // HSV fallback: green base rising through yellow to red, EQ-classic.
            [r, g, b] = hsvToRgb(0.33 - t01 * 0.33, sat, 1);
          }
          for (let ox = 0; ox <= 1; ox++) {
            for (let oy = 0; oy <= 1; oy++) {
              const i = idx(bx * 2 + ox, by * 2 + oy, z) * 3;
              buffer[i] = Math.round(r * cover);
              buffer[i + 1] = Math.round(g * cover);
              buffer[i + 2] = Math.round(b * cover);
            }
          }
        }
      }
    }
  },
};
