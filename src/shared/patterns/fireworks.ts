import { CUBE_N } from '../types';
import { num, clamp01 } from '../color';
import { getPalette } from '../palettes';
import type { Pattern } from './types';

/**
 * Fireworks.
 *
 * A white rocket flies from a random outer-edge launch point to a target near
 * the center of the cube, leaving a short white trail. On impact it explodes
 * into a burst of particles: each burst picks 1–2 colors from the active
 * palette so sequential explosions vary, and per-particle velocity / hue is
 * jittered. Particles are integrated in continuous (x,y,z) space and
 * trilinearly voxelized so sub-cell positions read as soft halos rather than
 * single hard LEDs.
 *
 * Audio:
 *   - beat  → trigger an extra immediate launch (with a short rate-limit)
 *   - level → scales particle count for ambient bursts
 */

const N = CUBE_N;

interface Vec3 { x: number; y: number; z: number }

interface Rocket {
  pos: Vec3;
  vel: Vec3;
  /** Time at which it should explode (ctx.t units). */
  explodeAt: number;
  /** Last K positions for the trail; head first. */
  trail: Vec3[];
}

interface Particle {
  pos: Vec3;
  vel: Vec3;
  color: [number, number, number];
  /** Spawn time (ctx.t). */
  born: number;
  /** Lifetime in seconds. */
  life: number;
}

interface FxState {
  rockets: Rocket[];
  particles: Particle[];
  nextLaunchT: number;
  /** Throttle for beat-triggered launches. */
  lastBeatLaunchT: number;
}

let fx: FxState | null = null;

function reset() {
  fx = {
    rockets: [],
    particles: [],
    nextLaunchT: 0,
    lastBeatLaunchT: -10,
  };
}

/**
 * Random launch point on the *outside* perimeter of the cube — i.e. on one of
 * the six faces, slightly outside the bounds so the rocket appears to enter.
 * Returns position and an aim point near the cube center.
 */
function randomLaunch(): { pos: Vec3; target: Vec3 } {
  const face = Math.floor(Math.random() * 6);
  const a = Math.random() * (N - 1);
  const b = Math.random() * (N - 1);
  // Sit ~1.5 cells outside the face so the rocket's first frames are below
  // the floor of the cube (looks like it's flying in from offscreen).
  const out = -1.5;
  const lo = out;
  const hi = N - 1 - out; // mirrored on the far side
  let pos: Vec3;
  switch (face) {
    case 0: pos = { x: lo, y: a, z: b }; break;       // -x face
    case 1: pos = { x: hi, y: a, z: b }; break;       // +x face
    case 2: pos = { x: a, y: lo, z: b }; break;       // -y face
    case 3: pos = { x: a, y: hi, z: b }; break;       // +y face
    case 4: pos = { x: a, y: b, z: lo }; break;       // -z face (most common in real fireworks)
    default: pos = { x: a, y: b, z: hi }; break;      // +z face
  }
  // Bias face selection toward -z so most rockets come up from below — feels
  // more like fireworks.
  if (Math.random() < 0.5) pos = { x: a, y: b, z: lo };

  // Target: somewhere in the middle 50% of the cube so explosions look
  // centered but not always on the same point.
  const c = (N - 1) / 2;
  const jitter = (N - 1) * 0.18;
  const target: Vec3 = {
    x: c + (Math.random() * 2 - 1) * jitter,
    y: c + (Math.random() * 2 - 1) * jitter,
    z: c + (Math.random() * 2 - 1) * jitter,
  };
  return { pos, target };
}

function launch(t: number, flightTime: number): Rocket {
  const { pos, target } = randomLaunch();
  const vel: Vec3 = {
    x: (target.x - pos.x) / flightTime,
    y: (target.y - pos.y) / flightTime,
    z: (target.z - pos.z) / flightTime,
  };
  return {
    pos: { ...pos },
    vel,
    explodeAt: t + flightTime,
    trail: [],
  };
}

/**
 * Spawn a burst of particles around `center`. Each burst picks two palette
 * stops and assigns each particle a hue interpolated between them, with a
 * small jitter so the burst doesn't look monochromatic.
 */
function spawnBurst(
  center: Vec3,
  count: number,
  speed: number,
  paletteName: string,
  t: number,
  life: number,
): Particle[] {
  const palette = getPalette(paletteName);
  const baseT = Math.random();
  const accentT = (baseT + 0.2 + Math.random() * 0.4) % 1;
  const baseColor = palette(baseT);
  const accentColor = palette(accentT);

  const out: Particle[] = [];
  for (let i = 0; i < count; i++) {
    // Direction: uniform on the unit sphere via Marsaglia method.
    let u: number, v: number, s: number;
    do {
      u = Math.random() * 2 - 1;
      v = Math.random() * 2 - 1;
      s = u * u + v * v;
    } while (s >= 1 || s === 0);
    const f = 2 * Math.sqrt(1 - s);
    const dx = u * f;
    const dy = v * f;
    const dz = 1 - 2 * s;

    // Speed jitter so particles spread into a fuzzy shell rather than a
    // perfect spherical front.
    const sp = speed * (0.55 + Math.random() * 0.9);

    // Color: mostly base, with a smaller proportion accent. Adds variety
    // within a single burst.
    const accent = Math.random() < 0.3;
    const c = accent ? accentColor : baseColor;
    // Per-particle slight brightness jitter so they don't all peak at once.
    const k = 0.75 + Math.random() * 0.5;
    const color: [number, number, number] = [
      Math.min(255, Math.round(c[0] * k)),
      Math.min(255, Math.round(c[1] * k)),
      Math.min(255, Math.round(c[2] * k)),
    ];

    out.push({
      pos: { ...center },
      vel: { x: dx * sp, y: dy * sp, z: dz * sp },
      color,
      born: t,
      life: life * (0.7 + Math.random() * 0.6),
    });
  }
  return out;
}

/**
 * Trilinearly accumulate a colored point at continuous (x,y,z) into the LED
 * buffer. Out-of-bounds samples are clipped per-corner so partial halos still
 * show as the particle leaves the cube.
 */
function splat(
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
        const wx = dx === 0 ? 1 - fx : fx;
        const w = wx * wy * wz;
        if (w <= 0.001) continue;
        const i = idx(cx, cy, cz) * 3;
        buffer[i]     = Math.min(255, buffer[i]     + Math.round(r * w));
        buffer[i + 1] = Math.min(255, buffer[i + 1] + Math.round(g * w));
        buffer[i + 2] = Math.min(255, buffer[i + 2] + Math.round(b * w));
      }
    }
  }
}

export const fireworks: Pattern = {
  meta: {
    id: 'fireworks',
    name: 'Fireworks',
    description: 'White rocket arcs in from an edge then bursts into palette-colored particles.',
    params: [
      { key: 'palette', label: 'Burst palette', type: 'palette', default: 'cyberpunk' },
      { key: 'launchInterval', label: 'Seconds between rockets', type: 'number', min: 0.3, max: 6, step: 0.1, default: 1.6 },
      { key: 'flightTime', label: 'Rocket flight (s)', type: 'number', min: 0.2, max: 2, step: 0.05, default: 0.7 },
      { key: 'particles', label: 'Particles per burst', type: 'number', min: 6, max: 200, step: 1, default: 70 },
      { key: 'burstSpeed', label: 'Burst speed (cells/s)', type: 'number', min: 1, max: 12, step: 0.25, default: 5.5 },
      { key: 'gravity', label: 'Gravity (z- per s)', type: 'number', min: 0, max: 8, step: 0.1, default: 2.5 },
      { key: 'drag', label: 'Drag (per s)', type: 'number', min: 0, max: 4, step: 0.05, default: 1.4 },
      { key: 'lifetime', label: 'Particle life (s)', type: 'number', min: 0.3, max: 4, step: 0.1, default: 1.4 },
      { key: 'rocketBrightness', label: 'Rocket brightness', type: 'number', min: 0, max: 1, step: 0.05, default: 0.95 },
      { key: 'beatLaunch', label: 'Beat launches a rocket', type: 'bool', default: true },
      { key: 'levelBoost', label: 'Level → particle boost', type: 'number', min: 0, max: 2, step: 0.05, default: 0.6 },
    ],
  },
  init(ctx) {
    reset();
    if (fx) fx.nextLaunchT = ctx.t + 0.3;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    if (!fx) reset();
    if (!fx) return;
    const { buffer, idx, t, dt, params, audio } = ctx;

    const paletteName = String(params.palette ?? 'cyberpunk');
    const launchInterval = Math.max(0.1, num(params.launchInterval, 1.6));
    const flightTime = Math.max(0.1, num(params.flightTime, 0.7));
    const baseParticles = Math.max(1, Math.floor(num(params.particles, 70)));
    const burstSpeed = num(params.burstSpeed, 5.5);
    const gravity = num(params.gravity, 2.5);
    const drag = Math.max(0, num(params.drag, 1.4));
    const lifetime = Math.max(0.1, num(params.lifetime, 1.4));
    const rocketB = clamp01(num(params.rocketBrightness, 0.95));
    const beatLaunch = Boolean(params.beatLaunch ?? true);
    const levelBoost = num(params.levelBoost, 0.6);

    const beat = audio?.beat ?? 0;
    const level = audio?.level ?? 0;

    // Schedule launches.
    if (t >= fx.nextLaunchT) {
      fx.rockets.push(launch(t, flightTime));
      fx.nextLaunchT = t + launchInterval;
    }
    if (beatLaunch && beat > 0.6 && t - fx.lastBeatLaunchT > 0.18) {
      fx.rockets.push(launch(t, flightTime * (0.7 + Math.random() * 0.4)));
      fx.lastBeatLaunchT = t;
    }

    // Step rockets.
    const survivingRockets: Rocket[] = [];
    for (const r of fx.rockets) {
      r.pos.x += r.vel.x * dt;
      r.pos.y += r.vel.y * dt;
      r.pos.z += r.vel.z * dt;
      r.trail.unshift({ x: r.pos.x, y: r.pos.y, z: r.pos.z });
      if (r.trail.length > 8) r.trail.length = 8;

      if (t >= r.explodeAt) {
        const burstScale = Math.round(baseParticles * (1 + level * levelBoost));
        const newParticles = spawnBurst(r.pos, burstScale, burstSpeed, paletteName, t, lifetime);
        fx.particles.push(...newParticles);
      } else {
        survivingRockets.push(r);
      }
    }
    fx.rockets = survivingRockets;

    // Step particles. Drop dead ones.
    const dragK = Math.exp(-drag * dt);
    const survivingParticles: Particle[] = [];
    for (const p of fx.particles) {
      p.vel.x *= dragK;
      p.vel.y *= dragK;
      p.vel.z = p.vel.z * dragK - gravity * dt;
      p.pos.x += p.vel.x * dt;
      p.pos.y += p.vel.y * dt;
      p.pos.z += p.vel.z * dt;
      const age = t - p.born;
      if (age >= p.life) continue;
      // Cull when far outside the cube — they're never coming back.
      if (
        p.pos.x < -3 || p.pos.x > N + 2 ||
        p.pos.y < -3 || p.pos.y > N + 2 ||
        p.pos.z < -3 || p.pos.z > N + 2
      ) continue;
      survivingParticles.push(p);
    }
    fx.particles = survivingParticles;

    // ───── render ─────
    buffer.fill(0);

    // Rockets: head bright white + short trail.
    for (const r of fx.rockets) {
      // Trail with falloff.
      const tl = r.trail.length;
      for (let i = 0; i < tl; i++) {
        const seg = r.trail[i];
        const k = (1 - i / tl) * rocketB;
        const v = Math.round(255 * k);
        splat(buffer, idx, seg.x, seg.y, seg.z, v, v, v);
      }
    }

    // Particles: brightness curve = peak at ~10% life, then fade quadratically.
    for (const p of fx.particles) {
      const age = t - p.born;
      const u = age / p.life;
      // Quick rise, slow fall.
      const env = u < 0.1 ? u / 0.1 : Math.pow(1 - (u - 0.1) / 0.9, 1.4);
      if (env <= 0.01) continue;
      const r = p.color[0] * env;
      const g = p.color[1] * env;
      const b = p.color[2] * env;
      splat(buffer, idx, p.pos.x, p.pos.y, p.pos.z, r, g, b);
    }
  },
};
