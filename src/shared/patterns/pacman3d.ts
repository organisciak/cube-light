import { CUBE_N } from '../types';
import { num, clamp01 } from '../color';
import { getPalette } from '../palettes';
import type { Pattern } from './types';

/**
 * 3D Pac-Man.
 *  - Cube starts fully lit with pellets. Pac-Man (bright yellow, chomping)
 *    moves cell-by-cell, eating each pellet it crosses.
 *  - Rainbow trail behind the head, fading with age along the chosen palette.
 *  - Manual: arrow keys / W S (same input bus as the snake — see server route).
 *  - Auto: BFS solver toward nearest pellet, weighted away from ghosts.
 *  - Audio: beat envelope speeds tick rate; level adds sparkle to remaining
 *    pellets and a brightness kick on the head.
 *  - Ghosts: 4 chasers with classic colors. On contact → death flash → reset.
 *  - Clear board → rainbow celebration → reset.
 */

export type PacDir = 'x+' | 'x-' | 'y+' | 'y-' | 'z+' | 'z-';

const N = CUBE_N;
const N3 = N * N * N;
const ALL_DIRS: PacDir[] = ['x+', 'x-', 'y+', 'y-', 'z+', 'z-'];

const STEP: Record<PacDir, [number, number, number]> = {
  'x+': [1, 0, 0], 'x-': [-1, 0, 0],
  'y+': [0, 1, 0], 'y-': [0, -1, 0],
  'z+': [0, 0, 1], 'z-': [0, 0, -1],
};

const OPPOSITE: Record<PacDir, PacDir> = {
  'x+': 'x-', 'x-': 'x+',
  'y+': 'y-', 'y-': 'y+',
  'z+': 'z-', 'z-': 'z+',
};

interface Cell { x: number; y: number; z: number }
interface Ghost extends Cell {
  color: [number, number, number];
  /** Last direction taken — discourages immediate reversal so ghosts don't dither. */
  lastDir: PacDir | null;
  /** Personal randomization phase so they don't all step in lockstep. */
  jitter: number;
}

const GHOST_COLORS: [number, number, number][] = [
  [255, 0, 0],     // Blinky
  [255, 184, 255], // Pinky
  [0, 255, 255],   // Inky
  [255, 184, 82],  // Clyde
];

interface PacGame {
  pac: Cell;
  dir: PacDir;
  pendingDir: PacDir | null;
  /** length-N3 byte array; 1 = pellet still present, 0 = eaten. */
  pellets: Uint8Array;
  pelletsLeft: number;
  /** Recent positions, head-first. Capped at maxTail (param). */
  trail: Cell[];
  ghosts: Ghost[];
  alive: boolean;
  /** Set when all pellets eaten — triggers a celebration before reset. */
  won: boolean;
  lastTickT: number;
  lastGhostT: number;
  deathStartT: number;
  winStartT: number;
  score: number;
  /** Beat-driven speed kick that decays exponentially. */
  beatBoost: number;
  /** Animation phase 0..1 for the chomp cycle. */
  chompPhase: number;
}

let game: PacGame | null = null;
let inputQueue: PacDir[] = [];

function cellIdx(x: number, y: number, z: number): number {
  return x + y * N + z * N * N;
}

function inBounds(x: number, y: number, z: number): boolean {
  return x >= 0 && x < N && y >= 0 && y < N && z >= 0 && z < N;
}

export function queuePacmanInput(dir: PacDir) {
  if (!game || !game.alive) return;
  if (dir === OPPOSITE[game.dir]) return;
  inputQueue.push(dir);
}

function reset() {
  const pellets = new Uint8Array(N3);
  pellets.fill(1);
  // Pac-Man starts at the center; that cell counts as eaten on spawn.
  const cx = Math.floor(N / 2);
  const cy = Math.floor(N / 2);
  const cz = Math.floor(N / 2);
  pellets[cellIdx(cx, cy, cz)] = 0;

  // Spawn ghosts in the four "lower" corners (z=0) so Pac-Man has a moment
  // before they catch up.
  const ghosts: Ghost[] = [
    { x: 0, y: 0, z: 0, color: GHOST_COLORS[0], lastDir: null, jitter: Math.random() },
    { x: N - 1, y: 0, z: 0, color: GHOST_COLORS[1], lastDir: null, jitter: Math.random() },
    { x: 0, y: N - 1, z: 0, color: GHOST_COLORS[2], lastDir: null, jitter: Math.random() },
    { x: N - 1, y: N - 1, z: 0, color: GHOST_COLORS[3], lastDir: null, jitter: Math.random() },
  ];
  for (const g of ghosts) pellets[cellIdx(g.x, g.y, g.z)] = 0;

  game = {
    pac: { x: cx, y: cy, z: cz },
    dir: 'x+',
    pendingDir: null,
    pellets,
    pelletsLeft: N3 - 1 - ghosts.length,
    trail: [],
    ghosts,
    alive: true,
    won: false,
    lastTickT: 0,
    lastGhostT: 0,
    deathStartT: 0,
    winStartT: 0,
    score: 0,
    beatBoost: 0,
    chompPhase: 0,
  };
  inputQueue = [];
}

export const pacman3d: Pattern = {
  meta: {
    id: 'pacman-3d',
    name: '3D Pac-Man',
    description: 'Eat every pellet. Beat speeds you up; ghosts chase. Same controls as Snake; "auto" plays itself.',
    params: [
      { key: 'mode', label: 'Mode', type: 'enum', options: ['auto', 'manual'], default: 'auto' },
      { key: 'baseSpeed', label: 'Pac-Man speed (Hz)', type: 'number', min: 1, max: 20, step: 0.5, default: 6 },
      { key: 'ghostSpeed', label: 'Ghost speed (Hz)', type: 'number', min: 0.5, max: 12, step: 0.25, default: 3 },
      { key: 'ghostCount', label: 'Ghost count', type: 'number', min: 0, max: 4, step: 1, default: 4 },
      { key: 'tailLength', label: 'Tail length', type: 'number', min: 0, max: 60, step: 1, default: 14 },
      { key: 'palette', label: 'Tail palette', type: 'palette', default: 'rainbow' },
      { key: 'pelletBrightness', label: 'Pellet brightness', type: 'number', min: 0, max: 1, step: 0.01, default: 0.06 },
      { key: 'pelletColor', label: 'Pellet hue (0=warm, 1=cool)', type: 'number', min: 0, max: 1, step: 0.01, default: 0.13 },
      { key: 'beatSpeedGain', label: 'Beat → speed kick', type: 'number', min: 0, max: 4, step: 0.1, default: 1.5 },
      { key: 'beatSparkle', label: 'Beat → pellet sparkle', type: 'number', min: 0, max: 1, step: 0.05, default: 0.6 },
      { key: 'levelGain', label: 'Level → ambient', type: 'number', min: 0, max: 2, step: 0.05, default: 0.5 },
      { key: 'wakaHz', label: 'Waka-waka rate (Hz)', type: 'number', min: 0, max: 12, step: 0.5, default: 5 },
      { key: 'ghostFearOnBeat', label: 'Ghosts flicker on beat', type: 'bool', default: true },
    ],
  },
  init(ctx) {
    reset();
    if (game) {
      game.lastTickT = ctx.t;
      game.lastGhostT = ctx.t;
    }
    ctx.buffer.fill(0);
  },
  render(ctx) {
    if (!game) reset();
    if (!game) return;
    const g = game;
    const { buffer, idx, t, dt, params, audio } = ctx;

    const mode = String(params.mode ?? 'auto') as 'auto' | 'manual';
    const baseSpeed = num(params.baseSpeed, 6);
    const ghostSpeed = num(params.ghostSpeed, 3);
    const ghostCount = Math.max(0, Math.min(4, Math.floor(num(params.ghostCount, 4))));
    const tailLen = Math.max(0, Math.floor(num(params.tailLength, 14)));
    const palette = getPalette(String(params.palette ?? 'rainbow'));
    const pelletB = clamp01(num(params.pelletBrightness, 0.06));
    const pelletHue = clamp01(num(params.pelletColor, 0.13));
    const beatSpeedGain = num(params.beatSpeedGain, 1.5);
    const beatSparkle = num(params.beatSparkle, 0.6);
    const levelGain = num(params.levelGain, 0.5);
    const wakaHz = num(params.wakaHz, 5);
    const ghostFearOnBeat = Boolean(params.ghostFearOnBeat ?? true);

    // --- audio integration ---
    const beat = audio?.beat ?? 0;
    const level = audio?.level ?? 0;
    // Beat boost decays exponentially. New beats clamp to whichever is larger.
    g.beatBoost = Math.max(g.beatBoost * Math.exp(-dt * 2.5), beat * beatSpeedGain);
    const speedMul = 1 + g.beatBoost;

    // --- ghost count adjustment (params can change live) ---
    while (g.ghosts.length > ghostCount) g.ghosts.pop();
    while (g.ghosts.length < ghostCount) {
      const i = g.ghosts.length;
      const corners: Cell[] = [
        { x: 0, y: 0, z: 0 },
        { x: N - 1, y: 0, z: 0 },
        { x: 0, y: N - 1, z: 0 },
        { x: N - 1, y: N - 1, z: 0 },
      ];
      const c = corners[i % corners.length];
      g.ghosts.push({ ...c, color: GHOST_COLORS[i % GHOST_COLORS.length], lastDir: null, jitter: Math.random() });
    }

    // --- Pac-Man tick ---
    if (g.alive && !g.won) {
      const tickInterval = 1 / Math.max(0.5, baseSpeed * speedMul);
      if (t - g.lastTickT >= tickInterval) {
        g.lastTickT = t;

        if (mode === 'auto') {
          g.pendingDir = autoChooseForPac(g);
          inputQueue = [];
        } else if (g.pendingDir == null && inputQueue.length > 0) {
          const next = inputQueue.shift()!;
          if (next !== OPPOSITE[g.dir]) g.pendingDir = next;
        }
        if (g.pendingDir) {
          g.dir = g.pendingDir;
          g.pendingDir = null;
        }

        const [dx, dy, dz] = STEP[g.dir];
        let nx = g.pac.x + dx;
        let ny = g.pac.y + dy;
        let nz = g.pac.z + dz;

        // Wall reflection: bounce off bounds in auto mode (Pac-Man doesn't die
        // from walls). In manual mode, also reflect — feels better than a hard
        // stop and avoids gameplay where you're forced to die into a wall.
        if (!inBounds(nx, ny, nz)) {
          g.dir = OPPOSITE[g.dir];
          const [dx2, dy2, dz2] = STEP[g.dir];
          nx = g.pac.x + dx2;
          ny = g.pac.y + dy2;
          nz = g.pac.z + dz2;
        }

        // Apply move.
        const prev = g.pac;
        g.pac = { x: nx, y: ny, z: nz };
        g.trail.unshift(prev);
        if (g.trail.length > tailLen) g.trail.length = tailLen;

        // Eat pellet.
        const cIdx = cellIdx(nx, ny, nz);
        if (g.pellets[cIdx]) {
          g.pellets[cIdx] = 0;
          g.pelletsLeft--;
          g.score++;
        }

        // Win check.
        if (g.pelletsLeft <= 0) {
          g.won = true;
          g.winStartT = t;
        }

        // Ghost-collision check after move.
        for (const gh of g.ghosts) {
          if (gh.x === nx && gh.y === ny && gh.z === nz) {
            g.alive = false;
            g.deathStartT = t;
            break;
          }
        }
      }
    }

    // --- Ghost tick (independent rate) ---
    if (g.alive && !g.won) {
      const ghostInterval = 1 / Math.max(0.25, ghostSpeed * (1 + g.beatBoost * 0.4));
      if (t - g.lastGhostT >= ghostInterval) {
        g.lastGhostT = t;
        stepGhosts(g);

        // Re-check post-ghost-step collision.
        for (const gh of g.ghosts) {
          if (gh.x === g.pac.x && gh.y === g.pac.y && gh.z === g.pac.z) {
            g.alive = false;
            g.deathStartT = t;
            break;
          }
        }
      }
    }

    // --- Win celebration / death cooldown ---
    if (g.won && t - g.winStartT > 2.0) reset();
    if (!g.alive && t - g.deathStartT > 1.5) reset();

    // ───── render ─────
    buffer.fill(0);

    // Pellet field — dim baseline, with a subtle sparkle when audio kicks.
    const pelletColor = hueToRgb(pelletHue);
    const ambient = pelletB * (1 + level * levelGain) + beat * beatSparkle * 0.3;
    if (ambient > 0.001) {
      // Scan only cells with pellets. 1000 cells is cheap.
      for (let z = 0; z < N; z++) {
        for (let y = 0; y < N; y++) {
          for (let x = 0; x < N; x++) {
            if (!g.pellets[cellIdx(x, y, z)]) continue;
            // Twinkle: small per-cell phase modulation tied to t.
            const twink = 0.85 + 0.15 * Math.sin(t * 4 + x * 1.7 + y * 2.3 + z * 0.9);
            const k = clamp01(ambient * twink);
            const i = idx(x, y, z) * 3;
            buffer[i] = Math.round(pelletColor[0] * k);
            buffer[i + 1] = Math.round(pelletColor[1] * k);
            buffer[i + 2] = Math.round(pelletColor[2] * k);
          }
        }
      }
    }

    if (g.won) {
      // Rainbow celebration: every cell gets a hue based on (t, position).
      const elapsed = t - g.winStartT;
      const fade = clamp01(1 - elapsed / 2.0);
      for (let z = 0; z < N; z++) {
        for (let y = 0; y < N; y++) {
          for (let x = 0; x < N; x++) {
            const h = (x + y + z) / (3 * (N - 1)) + t * 0.6;
            const [pr, pg, pb] = palette(((h % 1) + 1) % 1);
            const k = fade * (0.6 + 0.4 * Math.sin(t * 8 + x + y + z));
            const i = idx(x, y, z) * 3;
            buffer[i] = Math.round(pr * k);
            buffer[i + 1] = Math.round(pg * k);
            buffer[i + 2] = Math.round(pb * k);
          }
        }
      }
      return;
    }

    // Tail (rainbow), drawn before the head so the head wins on overlap.
    const tlen = g.trail.length;
    for (let s = 0; s < tlen; s++) {
      const seg = g.trail[s];
      // t01 = 1 next-to-head, falling to ~0 at the oldest.
      const t01 = 1 - s / Math.max(1, tlen);
      const [pr, pg, pb] = palette(((t01 + t * 0.3) % 1 + 1) % 1);
      const fall = 0.25 + 0.75 * t01;
      const i = idx(seg.x, seg.y, seg.z) * 3;
      buffer[i] = Math.round(pr * fall);
      buffer[i + 1] = Math.round(pg * fall);
      buffer[i + 2] = Math.round(pb * fall);
    }

    if (!g.alive) {
      // Death flash: red strobe at Pac-Man's last position.
      const elapsed = t - g.deathStartT;
      const fade = clamp01(1 - elapsed / 1.5);
      const strobe = Math.abs(Math.sin(elapsed * 18));
      const r = Math.round(255 * strobe * fade);
      const i = idx(g.pac.x, g.pac.y, g.pac.z) * 3;
      buffer[i] = r;
    } else {
      // Pac-Man: yellow with the chomping wave on top of an audio brightness kick.
      g.chompPhase = (g.chompPhase + dt * wakaHz) % 1;
      const chomp = 0.5 + 0.5 * Math.cos(g.chompPhase * Math.PI * 2);
      // Slightly redden when "mouth open" (chomp low) to suggest the inside.
      const yk = 0.55 + 0.45 * chomp + Math.min(0.4, level * 0.6) + Math.min(0.3, beat * 0.5);
      const yScale = clamp01(yk);
      const i = idx(g.pac.x, g.pac.y, g.pac.z) * 3;
      buffer[i] = Math.round(255 * yScale);
      buffer[i + 1] = Math.round((180 + 75 * chomp) * yScale);
      buffer[i + 2] = 0;
    }

    // Ghosts on top.
    const fearFlick = ghostFearOnBeat ? clamp01(beat) : 0;
    for (const gh of g.ghosts) {
      const i = idx(gh.x, gh.y, gh.z) * 3;
      // On a beat, ghosts briefly tint blue (the classic "scared" Pac-Man look).
      const [r, gC, b] = gh.color;
      const fr = Math.round(r * (1 - fearFlick) + 60 * fearFlick);
      const fg = Math.round(gC * (1 - fearFlick) + 60 * fearFlick);
      const fb = Math.round(b * (1 - fearFlick) + 255 * fearFlick);
      // Ghost overrides whatever was below.
      buffer[i] = fr;
      buffer[i + 1] = fg;
      buffer[i + 2] = fb;
    }
  },
};

function hueToRgb(h: number): [number, number, number] {
  // Map hue 0..1 to a soft pellet color (low-sat).
  // Keep saturation modest so pellets read as "dots" rather than competing
  // visually with Pac-Man and the ghosts.
  const hh = ((h % 1) + 1) % 1;
  const i = Math.floor(hh * 6);
  const f = hh * 6 - i;
  const s = 0.7;
  const v = 1;
  const p = v * (1 - s);
  const q = v * (1 - f * s);
  const w = v * (1 - (1 - f) * s);
  let r = 0, gC = 0, b = 0;
  switch (i % 6) {
    case 0: r = v; gC = w; b = p; break;
    case 1: r = q; gC = v; b = p; break;
    case 2: r = p; gC = v; b = w; break;
    case 3: r = p; gC = q; b = v; break;
    case 4: r = w; gC = p; b = v; break;
    case 5: r = v; gC = p; b = q; break;
  }
  return [Math.round(r * 255), Math.round(gC * 255), Math.round(b * 255)];
}

// ─── Pac-Man auto solver ────────────────────────────────────────────────────
//
// Strategy: BFS over the cube from Pac-Man's current cell, treating ghost
// cells (and a 1-cell halo around each) as blocked. Step toward the nearest
// pellet found. Falls back to "any legal direction that maximizes distance
// from the nearest ghost" if no pellet is reachable through safe space.

function autoChooseForPac(g: PacGame): PacDir {
  const start = g.pac;
  const blocked = new Uint8Array(N3);
  for (const gh of g.ghosts) {
    blocked[cellIdx(gh.x, gh.y, gh.z)] = 1;
    // Halo: also block one cell out in each axis to give ghosts a buffer
    // (otherwise Pac-Man tail-chases right into them on the next tick).
    for (const [dx, dy, dz] of [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]] as const) {
      const hx = gh.x + dx, hy = gh.y + dy, hz = gh.z + dz;
      if (inBounds(hx, hy, hz)) blocked[cellIdx(hx, hy, hz)] = 1;
    }
  }
  // Pac-Man's own start cell is allowed.
  blocked[cellIdx(start.x, start.y, start.z)] = 0;

  // BFS, recording first-step direction for each visited cell.
  const dist = new Int16Array(N3).fill(-1) as Int16Array;
  const firstDir = new Int8Array(N3); // index into ALL_DIRS, or -1
  for (let i = 0; i < N3; i++) firstDir[i] = -1;
  const sIdx = cellIdx(start.x, start.y, start.z);
  dist[sIdx] = 0;
  const queue: number[] = [sIdx];
  let head = 0;
  let bestPellet = -1;
  while (head < queue.length) {
    const cur = queue[head++];
    if (g.pellets[cur]) {
      bestPellet = cur;
      break;
    }
    const cx = cur % N;
    const cy = Math.floor(cur / N) % N;
    const cz = Math.floor(cur / (N * N));
    for (let d = 0; d < ALL_DIRS.length; d++) {
      const dir = ALL_DIRS[d];
      // Don't reverse on the first move — Pac-Man rarely benefits and it
      // looks indecisive.
      if (cur === sIdx && dir === OPPOSITE[g.dir]) continue;
      const [dx, dy, dz] = STEP[dir];
      const nx = cx + dx, ny = cy + dy, nz = cz + dz;
      if (!inBounds(nx, ny, nz)) continue;
      const ni = cellIdx(nx, ny, nz);
      if (dist[ni] !== -1) continue;
      if (blocked[ni]) continue;
      dist[ni] = dist[cur] + 1;
      firstDir[ni] = cur === sIdx ? d : firstDir[cur];
      queue.push(ni);
    }
  }

  if (bestPellet >= 0 && firstDir[bestPellet] >= 0) {
    return ALL_DIRS[firstDir[bestPellet]];
  }

  // Fallback: head wherever increases min-distance to ghosts the most.
  let bestDir: PacDir = g.dir;
  let bestScore = -Infinity;
  for (const dir of ALL_DIRS) {
    if (dir === OPPOSITE[g.dir]) continue;
    const [dx, dy, dz] = STEP[dir];
    const nx = start.x + dx, ny = start.y + dy, nz = start.z + dz;
    if (!inBounds(nx, ny, nz)) continue;
    if (blocked[cellIdx(nx, ny, nz)]) continue;
    let minGhost = Infinity;
    for (const gh of g.ghosts) {
      const d = Math.abs(gh.x - nx) + Math.abs(gh.y - ny) + Math.abs(gh.z - nz);
      if (d < minGhost) minGhost = d;
    }
    if (minGhost > bestScore) {
      bestScore = minGhost;
      bestDir = dir;
    }
  }
  return bestDir;
}

// ─── Ghost movement ─────────────────────────────────────────────────────────
//
// Greedy chase: pick the legal direction that minimizes Manhattan distance
// to Pac-Man, with a small chance of a random move for personality. Avoid
// reversing direction unless it's the only option.

function stepGhosts(g: PacGame) {
  const target = g.pac;
  for (const gh of g.ghosts) {
    const candidates: { dir: PacDir; cell: Cell; dist: number }[] = [];
    for (const dir of ALL_DIRS) {
      if (gh.lastDir && dir === OPPOSITE[gh.lastDir]) continue;
      const [dx, dy, dz] = STEP[dir];
      const nx = gh.x + dx, ny = gh.y + dy, nz = gh.z + dz;
      if (!inBounds(nx, ny, nz)) continue;
      // Ghosts are allowed to overlap each other and pellets, but not stack
      // on the same cell as another ghost — minor variety.
      if (g.ghosts.some((og) => og !== gh && og.x === nx && og.y === ny && og.z === nz)) continue;
      const dist =
        Math.abs(target.x - nx) +
        Math.abs(target.y - ny) +
        Math.abs(target.z - nz);
      candidates.push({ dir, cell: { x: nx, y: ny, z: nz }, dist });
    }
    if (candidates.length === 0) {
      // Stuck — try any direction including reverse.
      for (const dir of ALL_DIRS) {
        const [dx, dy, dz] = STEP[dir];
        const nx = gh.x + dx, ny = gh.y + dy, nz = gh.z + dz;
        if (!inBounds(nx, ny, nz)) continue;
        candidates.push({
          dir,
          cell: { x: nx, y: ny, z: nz },
          dist: Math.abs(target.x - nx) + Math.abs(target.y - ny) + Math.abs(target.z - nz),
        });
      }
      if (candidates.length === 0) continue;
    }
    candidates.sort((a, b) => a.dist - b.dist);
    // 15% wander to avoid 4 ghosts clumping into the same shortest path.
    const wander = Math.random() < 0.15;
    const chosen = wander ? candidates[Math.floor(Math.random() * candidates.length)] : candidates[0];
    gh.x = chosen.cell.x;
    gh.y = chosen.cell.y;
    gh.z = chosen.cell.z;
    gh.lastDir = chosen.dir;
  }
}
