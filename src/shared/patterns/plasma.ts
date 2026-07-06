import { CUBE_N } from '../types';
import { clamp01, hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

/**
 * Volumetric plasma: stacked sin waves in (x, y, z, t) produce a smooth
 * field that's read as a position into either the configured palette or,
 * when palette is 'none', the HSV ring with the saturation knob.
 */
export const plasma: Pattern = {
  meta: {
    id: 'plasma',
    name: 'Plasma',
    description: 'Smooth volumetric color field, palette-cycled. Audio bumps brightness.',
    params: [
      { key: 'speed', label: 'Speed', type: 'number', min: 0, max: 4, step: 0.05, default: 0.6 },
      { key: 'scale', label: 'Spatial scale', type: 'number', min: 1, max: 30, step: 0.5, default: 6 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'rainbow' },
      { key: 'hueShift', label: 'Palette shift', type: 'number', min: 0, max: 1, step: 0.01, default: 0 },
      { key: 'hueRange', label: 'Palette range', type: 'number', min: 0, max: 2, step: 0.01, default: 0.6 },
      { key: 'sat', label: 'Saturation (HSV mode)', type: 'number', min: 0, max: 1, step: 0.01, default: 1 },
      { key: 'bright', label: 'Brightness', type: 'number', min: 0, max: 1, step: 0.01, default: 0.6 },
      { key: 'audioGain', label: 'Audio brightness gain', type: 'number', min: 0, max: 3, step: 0.05, default: 0.6 },
    ],
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const N = CUBE_N;
    const speed = num(params.speed, 0.6);
    const scale = Math.max(0.5, num(params.scale, 6));
    const hueShift = num(params.hueShift, 0);
    const hueRange = num(params.hueRange, 0.6);
    const sat = clamp01(num(params.sat, 1));
    const baseBright = clamp01(num(params.bright, 0.6));
    const audioGain = num(params.audioGain, 0.6);
    const bright = clamp01(baseBright * (1 + audio.level * audioGain));
    const paletteName = String(params.palette ?? 'rainbow');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);
    const k = (Math.PI * 2) / scale;
    const wt = t * speed;

    for (let z = 0; z < N; z++) {
      for (let y = 0; y < N; y++) {
        for (let x = 0; x < N; x++) {
          const v =
            (Math.sin(k * x + wt * 1.0) +
              Math.sin(k * y + wt * 1.3) +
              Math.sin(k * z + wt * 0.7) +
              Math.sin(k * (x + y + z) * 0.33 + wt * 1.1)) /
              8 +
            0.5;
          const pos = hueShift + v * hueRange + wt * 0.05;
          let r: number, g: number, b: number;
          if (useP) {
            const [pr, pg, pb] = palette(((pos % 1) + 1) % 1);
            r = Math.round(pr * bright);
            g = Math.round(pg * bright);
            b = Math.round(pb * bright);
          } else {
            const hsv = hsvToRgb(pos, sat, bright);
            r = hsv[0]; g = hsv[1]; b = hsv[2];
          }
          const i = idx(x, y, z) * 3;
          buffer[i] = r;
          buffer[i + 1] = g;
          buffer[i + 2] = b;
        }
      }
    }
  },
};
