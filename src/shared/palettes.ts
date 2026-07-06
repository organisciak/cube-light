import { hsvToRgb } from './color';

/** Maps t∈[0,1] to an RGB triple in 0..255. */
export type Palette = (t: number) => [number, number, number];

function clamp01(t: number): number {
  return t < 0 ? 0 : t > 1 ? 1 : t;
}

function lerp(a: number, b: number, t: number): number {
  return a + (b - a) * t;
}

/**
 * Build a palette from an array of [position, [r,g,b]] stops, sampled with
 * linear interpolation. Positions must be ascending; first should be 0,
 * last should be 1.
 */
function gradient(stops: [number, [number, number, number]][]): Palette {
  return (t: number) => {
    const tt = clamp01(t);
    for (let i = 1; i < stops.length; i++) {
      const [pos, col] = stops[i];
      if (tt <= pos) {
        const [prevPos, prevCol] = stops[i - 1];
        const f = (tt - prevPos) / (pos - prevPos || 1);
        return [
          Math.round(lerp(prevCol[0], col[0], f)),
          Math.round(lerp(prevCol[1], col[1], f)),
          Math.round(lerp(prevCol[2], col[2], f)),
        ];
      }
    }
    return stops[stops.length - 1][1];
  };
}

export const palettes: Record<string, Palette> = {
  fire: gradient([
    [0.0, [0, 0, 0]],
    [0.2, [80, 0, 0]],
    [0.4, [255, 30, 0]],
    [0.65, [255, 150, 0]],
    [0.85, [255, 240, 60]],
    [1.0, [255, 255, 220]],
  ]),
  arctic: gradient([
    [0.0, [4, 8, 22]],
    [0.4, [30, 80, 180]],
    [0.7, [80, 200, 240]],
    [1.0, [240, 250, 255]],
  ]),
  sunset: gradient([
    [0.0, [40, 0, 60]],
    [0.3, [180, 30, 90]],
    [0.55, [255, 100, 60]],
    [0.8, [255, 200, 80]],
    [1.0, [255, 240, 200]],
  ]),
  forest: gradient([
    [0.0, [4, 12, 6]],
    [0.4, [10, 80, 20]],
    [0.75, [80, 200, 40]],
    [1.0, [220, 240, 120]],
  ]),
  ocean: gradient([
    [0.0, [0, 8, 30]],
    [0.4, [10, 50, 120]],
    [0.7, [40, 180, 200]],
    [1.0, [200, 255, 255]],
  ]),
  mono_red: gradient([
    [0.0, [0, 0, 0]],
    [0.5, [200, 30, 30]],
    [1.0, [255, 230, 200]],
  ]),
  mono_blue: gradient([
    [0.0, [0, 0, 0]],
    [0.5, [30, 80, 220]],
    [1.0, [220, 240, 255]],
  ]),
  rainbow: (t) => hsvToRgb(t, 1, 1),
  spectrum: gradient([
    [0.0, [200, 0, 200]],
    [0.25, [50, 0, 255]],
    [0.5, [0, 200, 255]],
    [0.75, [50, 255, 50]],
    [1.0, [255, 200, 0]],
  ]),
  cyberpunk: gradient([
    [0.0, [10, 0, 30]],
    [0.2, [120, 0, 140]],
    [0.4, [255, 30, 160]],
    [0.6, [180, 60, 255]],
    [0.8, [0, 220, 255]],
    [1.0, [180, 255, 240]],
  ]),
};

/**
 * Auto-cycling palette. Sweeps through the named palette list using
 * wall-clock time, with a brief crossfade at each boundary so transitions
 * read as smooth color drift rather than a flash. The output for a given
 * `t` depends on `Date.now()` — patterns sampling this palette will
 * therefore animate even if their own time input is held still.
 */
const CYCLE_LIST = ['cyberpunk', 'sunset', 'arctic', 'forest', 'spectrum', 'fire', 'ocean'];
const CYCLE_PERIOD_S = 8;
const CYCLE_FADE_S = 1.5;

palettes.cycle = (t: number) => {
  const now = Date.now() / 1000;
  const phase = now / CYCLE_PERIOD_S;
  const i = Math.floor(phase) % CYCLE_LIST.length;
  const j = (i + 1) % CYCLE_LIST.length;
  const localT = (phase - Math.floor(phase)) * CYCLE_PERIOD_S;
  const a = palettes[CYCLE_LIST[i]](t);
  if (localT <= CYCLE_PERIOD_S - CYCLE_FADE_S) return a;
  const mix = (localT - (CYCLE_PERIOD_S - CYCLE_FADE_S)) / CYCLE_FADE_S;
  const b = palettes[CYCLE_LIST[j]](t);
  return [
    Math.round(a[0] * (1 - mix) + b[0] * mix),
    Math.round(a[1] * (1 - mix) + b[1] * mix),
    Math.round(a[2] * (1 - mix) + b[2] * mix),
  ];
};

/** Sentinel: pattern should fall back to its native (RGB/HSV) color logic. */
export const PALETTE_NONE = 'none';

export const paletteNames = [PALETTE_NONE, ...Object.keys(palettes)];

export function getPalette(name: string | undefined | null): Palette {
  if (name && palettes[name]) return palettes[name];
  return palettes.rainbow;
}

export function isPaletteActive(name: string | undefined | null): boolean {
  return Boolean(name) && name !== PALETTE_NONE && Boolean(palettes[name as string]);
}
