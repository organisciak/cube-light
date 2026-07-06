import { CUBE_N } from '../types';
import { hsvToRgb, num } from '../color';
import { getPalette, isPaletteActive } from '../palettes';
import type { Pattern } from './types';

interface Ripple {
  age: number;
  /** Palette position 0..1 (also used as HSV hue when palette is 'none'). */
  pos: number;
  intensity: number;
}

let ripples: Ripple[] = [];
let lastT = 0;
let lastBeat = 0;
let lastLevel = 0;

/**
 * Concentric spheres expand from the cube center on every detected beat
 * (rising edge of the audio.beat envelope). Hue is biased by current band
 * energy, so different sounds emit different colors. With no audio, the
 * `autoSpawn` rate keeps it visually alive.
 */
export const audioRipple: Pattern = {
  meta: {
    id: 'audio-ripple',
    name: 'Audio Ripple',
    description: 'Spherical ripples emanate from center on every beat. Hue tracks band energy.',
    params: [
      { key: 'speed', label: 'Ripple speed (units/s)', type: 'number', min: 1, max: 15, step: 0.1, default: 5 },
      { key: 'thickness', label: 'Ring thickness', type: 'number', min: 0.4, max: 4, step: 0.1, default: 1 },
      { key: 'fade', label: 'Fade time (s)', type: 'number', min: 0.5, max: 5, step: 0.1, default: 2 },
      { key: 'autoSpawn', label: 'Auto-spawn rate (Hz)', type: 'number', min: 0, max: 4, step: 0.1, default: 0.4 },
      { key: 'beatThreshold', label: 'Beat trigger threshold', type: 'number', min: 0.1, max: 1, step: 0.05, default: 0.3 },
      { key: 'levelTrigger', label: 'Level-rise trigger', type: 'number', min: 0, max: 1, step: 0.02, default: 0.2 },
      { key: 'palette', label: 'Palette', type: 'palette', default: 'spectrum' },
      { key: 'sat', label: 'Saturation (HSV mode)', type: 'number', min: 0, max: 1, step: 0.05, default: 0.85 },
    ],
  },
  init(ctx) {
    ripples = [];
    lastT = ctx.t;
    lastBeat = 0;
    lastLevel = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params, audio } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const speed = num(params.speed, 5);
    const thickness = Math.max(0.2, num(params.thickness, 1));
    const fade = Math.max(0.5, num(params.fade, 2));
    const autoSpawnRate = num(params.autoSpawn, 0);
    const beatThreshold = num(params.beatThreshold, 0.3);
    const levelTrigger = num(params.levelTrigger, 0.2);
    const sat = num(params.sat, 0.85);
    const paletteName = String(params.palette ?? 'spectrum');
    const useP = isPaletteActive(paletteName);
    const palette = getPalette(paletteName);

    // Spawn on rising edge of beat envelope, OR on a sharp rise in overall
    // level (catches fast transients even when the beat detector misses).
    const beat = audio.beat ?? 0;
    const level = audio.level ?? 0;
    const beatRising = beat > beatThreshold && lastBeat <= beatThreshold;
    const levelRising = levelTrigger > 0 && level - lastLevel > levelTrigger;
    if (beatRising || levelRising) {
      const bass = audio.bands[0] ?? 0;
      const mid = audio.bands[Math.floor(audio.bands.length / 2)] ?? 0;
      const treble = audio.bands[audio.bands.length - 1] ?? 0;
      const total = bass + mid + treble + 1e-6;
      // Position weighted by band balance: bass→start, mid→middle, treble→end.
      const pos = (bass * 0.0 + mid * 0.5 + treble * 1.0) / total;
      ripples.push({ age: 0, pos, intensity: 0.7 + 0.3 * level });
    }
    lastBeat = beat;
    // Decay the cached level slowly so the rising-edge detector keeps firing
    // on each new transient rather than once at the start of a loud section.
    lastLevel = Math.max(level, lastLevel - dt * 1.5);

    if (autoSpawnRate > 0 && Math.random() < autoSpawnRate * dt) {
      ripples.push({ age: 0, pos: Math.random(), intensity: 0.7 });
    }

    buffer.fill(0);
    const c = (N - 1) / 2;

    const next: Ripple[] = [];
    for (const rp of ripples) {
      rp.age += dt;
      if (rp.age > fade) continue;
      const rad = rp.age * speed;
      if (rad > N * 1.8) continue;
      const lifeFrac = 1 - rp.age / fade;
      const intensity = rp.intensity * lifeFrac;
      const p = ((rp.pos % 1) + 1) % 1;
      let rr: number, gg: number, bb: number;
      if (useP) {
        const [pr, pg, pb] = palette(p);
        rr = Math.round(pr * intensity);
        gg = Math.round(pg * intensity);
        bb = Math.round(pb * intensity);
      } else {
        const hsv = hsvToRgb(p, sat, intensity);
        rr = hsv[0]; gg = hsv[1]; bb = hsv[2];
      }

      for (let z = 0; z < N; z++) {
        for (let y = 0; y < N; y++) {
          for (let x = 0; x < N; x++) {
            const dx = x - c;
            const dy = y - c;
            const dz = z - c;
            const dist = Math.sqrt(dx * dx + dy * dy + dz * dz);
            const d = Math.abs(dist - rad);
            if (d > thickness) continue;
            const fall = 1 - d / thickness;
            const i = idx(x, y, z) * 3;
            buffer[i] = Math.max(buffer[i], Math.round(rr * fall));
            buffer[i + 1] = Math.max(buffer[i + 1], Math.round(gg * fall));
            buffer[i + 2] = Math.max(buffer[i + 2], Math.round(bb * fall));
          }
        }
      }
      next.push(rp);
    }
    ripples = next;
  },
};
