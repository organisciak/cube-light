import { CUBE_N } from '../types';
import { clamp01, num } from '../color';
import type { Pattern } from './types';

type RGB = [number, number, number];

function lerpC(a: RGB, b: RGB, t: number): RGB {
  return [
    a[0] + (b[0] - a[0]) * t,
    a[1] + (b[1] - a[1]) * t,
    a[2] + (b[2] - a[2]) * t,
  ];
}

// Ambient sky color keyed by time-of-day (0=midnight, 0.25=sunrise,
// 0.5=noon, 0.75=sunset, 1=midnight). Localized warm/cool halos around
// the sun and moon are added on top of this per cell.
const SKY_STOPS: [number, RGB][] = [
  [0.00, [4, 6, 24]],
  [0.20, [50, 30, 80]],
  [0.27, [180, 90, 60]],
  [0.35, [110, 165, 225]],
  [0.50, [60, 140, 240]],
  [0.65, [110, 160, 220]],
  [0.73, [210, 100, 50]],
  [0.80, [60, 30, 80]],
  [1.00, [4, 6, 24]],
];

function sampleStops(stops: [number, RGB][], t: number): RGB {
  const tt = ((t % 1) + 1) % 1;
  for (let i = 1; i < stops.length; i++) {
    if (tt <= stops[i][0]) {
      const [p0, c0] = stops[i - 1];
      const [p1, c1] = stops[i];
      const f = (tt - p0) / (p1 - p0 || 1);
      return lerpC(c0, c1, f);
    }
  }
  return stops[stops.length - 1][1];
}

let lastT = 0;
let cycleT = 0;

/**
 * A green-hill landscape under a sky that cycles through dawn, noon, dusk
 * and night. The sun arcs from east (x=0) up over the cube and down to the
 * west (x=N-1), tracing a half-circle in the xy plane (z held at center).
 * The moon trails half a cycle behind. Both contribute warm/cool halos to
 * surrounding sky cells. The hill is a paraboloid mask shaded by the
 * sun/moon — vivid green at noon, dim and warm at twilight, deep blue-green
 * at night.
 *
 * Use mode='manual' to scrub through the day with the time slider.
 */
export const dayCycle: Pattern = {
  meta: {
    id: 'day-cycle',
    name: 'Day & Night',
    description: 'Green hills under a sky that cycles dawn → noon → dusk → night, with sun and moon arcs. Auto or manual time-of-day.',
    params: [
      { key: 'upAxis', label: 'Up axis', type: 'enum', options: ['x', 'y', 'z'], default: 'z' },
      { key: 'mode', label: 'Mode', type: 'enum', options: ['auto', 'manual'], default: 'auto' },
      { key: 'time', label: 'Time of day (manual)', type: 'number', min: 0, max: 1, step: 0.005, default: 0.5 },
      { key: 'cycleSec', label: 'Cycle length (s, auto)', type: 'number', min: 10, max: 600, step: 1, default: 60 },
      { key: 'peak', label: 'Hill peak height', type: 'number', min: 1, max: 8, step: 1, default: 4 },
      { key: 'hillR', label: 'Hill R (noon)', type: 'number', min: 0, max: 255, step: 1, default: 50 },
      { key: 'hillG', label: 'Hill G (noon)', type: 'number', min: 0, max: 255, step: 1, default: 200 },
      { key: 'hillB', label: 'Hill B (noon)', type: 'number', min: 0, max: 255, step: 1, default: 70 },
      { key: 'sunSize', label: 'Sun glow size', type: 'number', min: 0.5, max: 8, step: 0.1, default: 2.5 },
      { key: 'moonBright', label: 'Moonlight strength', type: 'number', min: 0, max: 1, step: 0.05, default: 0.5 },
    ],
  },
  init(ctx) {
    lastT = ctx.t;
    cycleT = 0;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    const { buffer, idx, t, params } = ctx;
    const dt = Math.max(0, Math.min(0.1, t - lastT));
    lastT = t;

    const N = CUBE_N;
    const c = (N - 1) / 2;
    const upAxis = (String(params.upAxis ?? 'z') as 'x' | 'y' | 'z');
    const peak = num(params.peak, 4);
    const noonR = num(params.hillR, 50);
    const noonG = num(params.hillG, 200);
    const noonB = num(params.hillB, 70);
    const sunSize = Math.max(0.5, num(params.sunSize, 2.5));
    const moonBright = clamp01(num(params.moonBright, 0.5));
    const mode = String(params.mode ?? 'auto');

    let tod: number;
    if (mode === 'manual') {
      tod = ((num(params.time, 0.5) % 1) + 1) % 1;
    } else {
      const cycleSec = Math.max(1, num(params.cycleSec, 60));
      cycleT = (cycleT + dt / cycleSec) % 1;
      tod = cycleT;
    }

    // Celestial geometry, written in abstract (up, east, side) coords.
    // Sun rides a half-circle in the (up, east) plane (east horizon → up
    // → west horizon); altitude = sin(angle) is positive during day,
    // negative below horizon at night. Side axis is held at center.
    const angle = (tod - 0.25) * Math.PI * 2;
    const sunAlt = Math.sin(angle);
    const arcR = c + 0.8;
    const sunU = c + arcR * Math.sin(angle);
    const sunE = c + arcR * Math.cos(angle);
    const sunS = c;
    const mAngle = angle + Math.PI;
    const moonAlt = Math.sin(mAngle);
    const moonU = c + arcR * Math.sin(mAngle);
    const moonE = c + arcR * Math.cos(mAngle);
    const moonS = c;

    // World lighting: dayness ramps from 0 below horizon to 1 once sun
    // clears low altitude. Twilight peaks when sun is near horizon.
    const dayness = clamp01((sunAlt + 0.15) / 0.4);
    const twilight = clamp01(1 - Math.abs(sunAlt) / 0.25);
    const moonLight = Math.max(0, moonAlt) * moonBright;

    const skyBase = sampleStops(SKY_STOPS, tod);
    // Sun core color: warm at horizon, near-white at zenith.
    const sunWarm: RGB = [255, 170, 80];
    const sunBright: RGB = [255, 240, 200];
    const sunColor = lerpC(sunWarm, sunBright, clamp01(sunAlt));
    const moonColor: RGB = [220, 230, 255];

    // Lit hill color = noon green attenuated by overall lighting, plus a
    // twilight warm tint and a cool moonlight tint when active.
    const lighting = 0.05 + 0.95 * dayness + 0.35 * moonLight;
    let hillR = noonR * lighting;
    let hillG = noonG * lighting;
    let hillB = noonB * lighting;
    hillR += 50 * twilight;
    hillG -= 30 * twilight;
    hillR += 8 * moonLight;
    hillB += 30 * moonLight;
    const hillLit: RGB = [hillR, hillG, hillB];

    // Paraboloid hill mask: at the cube center the hill rises to `peak`,
    // tapering smoothly to zero a bit beyond the corners so the bottom
    // plane is fully covered.
    const hillBaseRSq = (Math.hypot(c, c) * 1.2) ** 2;

    // Map abstract (up, east, side) → physical (x, y, z) for the chosen
    // up axis. Whichever physical axis is "up" carries hill height & sun
    // altitude; the other two are the horizontal east/side plane.
    const getIdx = (u: number, e: number, s: number): number => {
      if (upAxis === 'z') return idx(e, s, u);
      if (upAxis === 'x') return idx(u, e, s);
      return idx(e, u, s);
    };

    buffer.fill(0);

    for (let u = 0; u < N; u++) {
      for (let e = 0; e < N; e++) {
        for (let s = 0; s < N; s++) {
          const de = e - c;
          const ds = s - c;
          const dHillSq = de * de + ds * ds;
          const heightHere = peak * Math.max(0, 1 - dHillSq / hillBaseRSq);
          // hillT in 0..1: 1 = solidly inside hill, 0 = clearly above it,
          // intermediate at the soft top edge.
          const hillT = clamp01(heightHere - u + 0.5);

          let r = skyBase[0];
          let g = skyBase[1];
          let b = skyBase[2];

          // Sun contribution (visible while not deep below horizon).
          if (sunAlt > -0.3) {
            const su = u - sunU, se = e - sunE, ss = s - sunS;
            const sd = Math.hypot(su, se, ss);
            const horizonFade = sunAlt > 0 ? 1 : Math.max(0, (sunAlt + 0.3) / 0.3);
            const glow = Math.exp(-sd / sunSize) * horizonFade;
            const core = Math.max(0, 1.8 - sd) * (sunAlt > -0.1 ? 1 : 0);
            const sunMix = clamp01(glow * 0.9 + core);
            r = r * (1 - sunMix) + sunColor[0] * sunMix;
            g = g * (1 - sunMix) + sunColor[1] * sunMix;
            b = b * (1 - sunMix) + sunColor[2] * sunMix;
          }

          // Moon contribution.
          if (moonAlt > -0.1 && moonBright > 0) {
            const mu = u - moonU, me = e - moonE, ms = s - moonS;
            const md = Math.hypot(mu, me, ms);
            const horizonFade = clamp01((moonAlt + 0.1) / 0.2);
            const mGlow = Math.exp(-md / (sunSize * 0.7)) * horizonFade;
            const mCore = Math.max(0, 1.5 - md) * horizonFade;
            const mMix = clamp01((mGlow * 0.5 + mCore) * moonBright);
            r = r * (1 - mMix) + moonColor[0] * mMix;
            g = g * (1 - mMix) + moonColor[1] * mMix;
            b = b * (1 - mMix) + moonColor[2] * mMix;
          }

          // Blend toward hill where the mask is set.
          if (hillT > 0) {
            r = r * (1 - hillT) + hillLit[0] * hillT;
            g = g * (1 - hillT) + hillLit[1] * hillT;
            b = b * (1 - hillT) + hillLit[2] * hillT;
          }

          const i = getIdx(u, e, s) * 3;
          buffer[i] = Math.max(0, Math.min(255, Math.round(r)));
          buffer[i + 1] = Math.max(0, Math.min(255, Math.round(g)));
          buffer[i + 2] = Math.max(0, Math.min(255, Math.round(b)));
        }
      }
    }
  },
};
