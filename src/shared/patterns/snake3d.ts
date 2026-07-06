import { CUBE_N } from '../types';
import { num } from '../color';
import { getPalette } from '../palettes';
import type { Pattern } from './types';

/**
 * 3D snake game.
 *  - Arrow keys: x/y movement (Up=+y, Down=-y, Left=-x, Right=+x).
 *  - W / S: +z / -z.
 *  - Eat apples to grow tail and speed up.
 *  - Wall or self collision → death flash → restart at length 3.
 *
 * Game state lives module-locally; init() resets it. Input arrives via the
 * exported queueSnakeInput function (the server's WS handler calls it).
 */

export type SnakeDir = 'x+' | 'x-' | 'y+' | 'y-' | 'z+' | 'z-';

const N = CUBE_N;

const STEP: Record<SnakeDir, [number, number, number]> = {
  'x+': [1, 0, 0],
  'x-': [-1, 0, 0],
  'y+': [0, 1, 0],
  'y-': [0, -1, 0],
  'z+': [0, 0, 1],
  'z-': [0, 0, -1],
};

const OPPOSITE: Record<SnakeDir, SnakeDir> = {
  'x+': 'x-', 'x-': 'x+',
  'y+': 'y-', 'y-': 'y+',
  'z+': 'z-', 'z-': 'z+',
};

interface Segment { x: number; y: number; z: number }

interface SnakeGame {
  segments: Segment[];   // head first
  direction: SnakeDir;
  pendingDir: SnakeDir | null;
  apple: Segment;
  lastTickT: number;
  alive: boolean;
  /** False until the player has supplied a first input. While false, snake is idle. */
  started: boolean;
  deathStartT: number;
  score: number;
  highScore: number;
  /** Most recent input direction, for the training hint. Null = nothing to show. */
  hintDir: SnakeDir | null;
  /** When the current hint flash started. Game time (ctx.t) units. */
  hintStartT: number;
}

let game: SnakeGame | null = null;
let inputQueue: SnakeDir[] = [];
/** Set by the WS handler; render() consumes and stamps the in-game time. */
let pendingHint: SnakeDir | null = null;

export function queueSnakeInput(dir: SnakeDir) {
  if (!game || !game.alive) return;
  // Drop reverse-of-current so a queued input that'd kill the snake never
  // sees a chance to apply.
  if (dir === OPPOSITE[game.direction]) return;
  if (!game.started) {
    // First input both starts movement AND sets initial direction.
    // Don't enqueue — the dir is applied directly to game.direction.
    game.started = true;
    game.direction = dir;
  } else {
    inputQueue.push(dir);
  }
  // Always flash the hint, even on the kick-off press.
  pendingHint = dir;
}

export function getSnakeState(): { score: number; highScore: number; alive: boolean; length: number } | null {
  if (!game) return null;
  return {
    score: game.score,
    highScore: game.highScore,
    alive: game.alive,
    length: game.segments.length,
  };
}

function randomEmpty(occupied: Segment[]): Segment {
  const set = new Set(occupied.map((s) => `${s.x},${s.y},${s.z}`));
  for (let attempt = 0; attempt < 2000; attempt++) {
    const x = Math.floor(Math.random() * N);
    const y = Math.floor(Math.random() * N);
    const z = Math.floor(Math.random() * N);
    if (!set.has(`${x},${y},${z}`)) return { x, y, z };
  }
  return { x: 0, y: 0, z: 0 };
}

function reset(prevHighScore: number) {
  const cx = Math.floor(N / 2);
  const cy = Math.floor(N / 2);
  const cz = Math.floor(N / 2);
  const segments: Segment[] = [
    { x: cx, y: cy, z: cz },
    { x: cx - 1, y: cy, z: cz },
    { x: cx - 2, y: cy, z: cz },
  ];
  game = {
    segments,
    direction: 'x+',
    pendingDir: null,
    apple: randomEmpty(segments),
    lastTickT: 0,
    alive: true,
    started: false,
    deathStartT: 0,
    score: 0,
    highScore: prevHighScore,
    hintDir: null,
    hintStartT: -999,
  };
  inputQueue = [];
  pendingHint = null;
}

function speedHz(score: number, base: number, max: number, lengthForMax: number): number {
  const t = Math.min(1, score / Math.max(1, lengthForMax));
  return base + (max - base) * t;
}

export const snake3d: Pattern = {
  meta: {
    id: 'snake-3d',
    name: '3D Snake',
    description: 'Arrow keys / A D = x, ↑ ↓ = y, W S = z. Auto mode runs a CPU solver.',
    params: [
      { key: 'mode', label: 'Mode', type: 'enum', options: ['manual', 'auto'], default: 'manual' },
      { key: 'nearMiss', label: 'Auto: near-miss chance', type: 'number', min: 0, max: 1, step: 0.05, default: 0 },
      { key: 'baseSpeed', label: 'Start speed (Hz)', type: 'number', min: 1, max: 12, step: 0.5, default: 1 },
      { key: 'maxSpeed', label: 'Max speed (Hz)', type: 'number', min: 4, max: 25, step: 0.5, default: 12 },
      { key: 'lengthForMax', label: 'Length for max speed', type: 'number', min: 5, max: 60, step: 1, default: 25 },
      { key: 'palette', label: 'Tail palette', type: 'palette', default: 'spectrum' },
      { key: 'appleR', label: 'Apple R', type: 'number', min: 0, max: 255, step: 1, default: 255 },
      { key: 'appleG', label: 'Apple G', type: 'number', min: 0, max: 255, step: 1, default: 40 },
      { key: 'appleB', label: 'Apple B', type: 'number', min: 0, max: 255, step: 1, default: 40 },
      { key: 'hintFullScore', label: 'Hint full until score', type: 'number', min: 0, max: 20, step: 1, default: 3 },
      { key: 'hintGoneScore', label: 'Hint gone by score', type: 'number', min: 1, max: 30, step: 1, default: 8 },
      { key: 'hintFlashSec', label: 'Hint flash duration (s)', type: 'number', min: 0.1, max: 1.5, step: 0.05, default: 0.45 },
    ],
  },
  init(ctx) {
    reset(game?.highScore ?? 0);
    if (game) game.lastTickT = ctx.t;
    ctx.buffer.fill(0);
  },
  render(ctx) {
    if (!game) reset(0);
    if (!game) return;
    const { buffer, idx, t, params } = ctx;
    const mode = String(params.mode ?? 'manual') as 'manual' | 'auto';
    const nearMiss = Math.max(0, Math.min(1, num(params.nearMiss, 0)));
    const baseSpeed = num(params.baseSpeed, 4);
    const maxSpeed = num(params.maxSpeed, 12);
    const lengthForMax = num(params.lengthForMax, 25);
    const palette = getPalette(String(params.palette ?? 'spectrum'));
    const appleR = num(params.appleR, 255);
    const appleG = num(params.appleG, 40);
    const appleB = num(params.appleB, 40);
    const hintFullScore = num(params.hintFullScore, 3);
    const hintGoneScore = Math.max(hintFullScore + 1, num(params.hintGoneScore, 8));
    const hintFlashSec = Math.max(0.05, num(params.hintFlashSec, 0.45));

    // Auto mode: bypass the "wait for first input" gate so the demo starts
    // moving immediately when the user picks the pattern.
    if (mode === 'auto' && !game.started && game.alive) {
      game.started = true;
    }

    // Stamp incoming hint with the current game-time (queueSnakeInput can't see ctx.t).
    if (pendingHint != null) {
      game.hintDir = pendingHint;
      game.hintStartT = t;
      pendingHint = null;
    }

    // Pull at most one queued input per tick — burst presses shouldn't all
    // apply on the same tick.
    if (game.pendingDir == null && inputQueue.length > 0) {
      const next = inputQueue.shift()!;
      if (next !== OPPOSITE[game.direction]) {
        game.pendingDir = next;
      }
    }

    if (game.alive && game.started) {
      const speed = speedHz(game.score, baseSpeed, maxSpeed, lengthForMax);
      const tickInterval = 1 / Math.max(0.5, speed);

      if (t - game.lastTickT >= tickInterval) {
        game.lastTickT = t;

        // In auto mode, pick the next direction with the CPU solver before
        // applying any pending input. Manual queued inputs are ignored while
        // auto is on, so toggling between modes mid-game is clean.
        if (mode === 'auto') {
          game.pendingDir = autoChoose(game, nearMiss);
          inputQueue = [];
        }

        if (game.pendingDir) {
          game.direction = game.pendingDir;
          game.pendingDir = null;
        }

        const [dx, dy, dz] = STEP[game.direction];
        const head = game.segments[0];
        const newHead = { x: head.x + dx, y: head.y + dy, z: head.z + dz };

        const outOfBounds =
          newHead.x < 0 || newHead.x >= N ||
          newHead.y < 0 || newHead.y >= N ||
          newHead.z < 0 || newHead.z >= N;

        if (outOfBounds) {
          game.alive = false;
          game.deathStartT = t;
        } else {
          const ate =
            newHead.x === game.apple.x &&
            newHead.y === game.apple.y &&
            newHead.z === game.apple.z;
          // The tail tip is about to move (unless eating), so don't check it.
          const body = ate ? game.segments : game.segments.slice(0, -1);
          const selfHit = body.some((s) => s.x === newHead.x && s.y === newHead.y && s.z === newHead.z);
          if (selfHit) {
            game.alive = false;
            game.deathStartT = t;
          } else {
            game.segments.unshift(newHead);
            if (ate) {
              game.score++;
              game.highScore = Math.max(game.highScore, game.score);
              game.apple = randomEmpty(game.segments);
            } else {
              game.segments.pop();
            }
          }
        }
      }
    } else if (!game.alive) {
      const elapsed = t - game.deathStartT;
      if (elapsed > 1.5) reset(game.highScore);
    }
    // (when alive but !started: snake idles in place; tick logic skipped)

    buffer.fill(0);

    if (!game.alive) {
      // Death flash: red strobe along the snake, fading.
      const elapsed = t - game.deathStartT;
      const fade = Math.max(0, 1 - elapsed / 1.5);
      const strobe = Math.abs(Math.sin(elapsed * 14));
      const r = Math.round(255 * strobe * fade);
      for (const s of game.segments) {
        const i = idx(s.x, s.y, s.z) * 3;
        buffer[i] = r;
      }
    } else {
      const len = game.segments.length;
      for (let s = 0; s < len; s++) {
        const seg = game.segments[s];
        // Head=1.0 → tail tip=0.05 along the palette.
        const t01 = 1 - s / Math.max(1, len);
        const [pr, pg, pb] = palette(t01);
        const fall = 0.35 + 0.65 * t01;
        const i = idx(seg.x, seg.y, seg.z) * 3;
        buffer[i] = Math.round(pr * fall);
        buffer[i + 1] = Math.round(pg * fall);
        buffer[i + 2] = Math.round(pb * fall);
      }
      // Apple: gentle pulse.
      const pulse = 0.65 + 0.35 * Math.sin(t * 6);
      const ai = idx(game.apple.x, game.apple.y, game.apple.z) * 3;
      buffer[ai] = Math.round(appleR * pulse);
      buffer[ai + 1] = Math.round(appleG * pulse);
      buffer[ai + 2] = Math.round(appleB * pulse);

      // Direction-hint training overlay: a 3×3 patch on the far face of the
      // pressed axis, centered on the head's other-axis coords. Fades both
      // by time-since-press and by accumulated score, so it's gone after the
      // player has internalized the controls.
      if (game.hintDir) {
        const scoreF =
          game.score < hintFullScore
            ? 1
            : Math.max(0, 1 - (game.score - hintFullScore) / (hintGoneScore - hintFullScore));
        const flashF = Math.max(0, 1 - (t - game.hintStartT) / hintFlashSec);
        const intensity = scoreF * flashF;
        if (intensity > 0.02) {
          drawDirectionHint(buffer, idx, game.hintDir, game.segments[0], intensity);
        }
      }
    }
  },
};

/**
 * Draw a 3×3 patch on the far face of the cube along the requested axis,
 * centered on the head's other-axis coords. Uses an axis color (x=red,
 * y=green, z=blue) — same scheme as the orientation buttons in the UI.
 */
function drawDirectionHint(
  buffer: Uint8Array,
  idx: (x: number, y: number, z: number) => number,
  dir: SnakeDir,
  head: Segment,
  intensity: number,
) {
  const axis = dir[0] as 'x' | 'y' | 'z';
  const sign = dir[1] === '+' ? 1 : -1;
  const fixed = sign > 0 ? N - 1 : 0;
  const color: [number, number, number] =
    axis === 'x' ? [255, 60, 60] :
    axis === 'y' ? [70, 230, 70] :
                   [90, 140, 255];

  // Patch span: 3×3 on the two axes that aren't `axis`.
  const range = (c: number) => {
    const lo = Math.max(0, c - 1);
    const hi = Math.min(N - 1, c + 1);
    const out: number[] = [];
    for (let v = lo; v <= hi; v++) out.push(v);
    return out;
  };

  const cells: [number, number, number][] = [];
  if (axis === 'x') {
    for (const y of range(head.y)) for (const z of range(head.z)) cells.push([fixed, y, z]);
  } else if (axis === 'y') {
    for (const x of range(head.x)) for (const z of range(head.z)) cells.push([x, fixed, z]);
  } else {
    for (const x of range(head.x)) for (const y of range(head.y)) cells.push([x, y, fixed]);
  }

  for (const [x, y, z] of cells) {
    const i = idx(x, y, z) * 3;
    // Additive blend with whatever's already there (e.g., snake passing through).
    buffer[i] = Math.min(255, buffer[i] + Math.round(color[0] * intensity));
    buffer[i + 1] = Math.min(255, buffer[i + 1] + Math.round(color[1] * intensity));
    buffer[i + 2] = Math.min(255, buffer[i + 2] + Math.round(color[2] * intensity));
  }
}

// ─── CPU solver ─────────────────────────────────────────────────────────────

const N3 = N * N * N;
const ALL_DIRS: SnakeDir[] = ['x+', 'x-', 'y+', 'y-', 'z+', 'z-'];

function cellIdx(x: number, y: number, z: number): number {
  return x + y * N + z * N * N;
}

/**
 * Build a Uint8Array of cube cells where 1 = blocked by snake body. Includes
 * the head; the caller is responsible for excluding cells if needed.
 */
function buildOccupancy(body: Segment[]): Uint8Array {
  const occ = new Uint8Array(N3);
  for (const s of body) occ[cellIdx(s.x, s.y, s.z)] = 1;
  return occ;
}

/**
 * BFS shortest-path distance from `start` to `target` through unblocked cells.
 * `target` itself can be passable. Returns Infinity if unreachable.
 */
function bfsDist(occ: Uint8Array, start: Segment, target: Segment): number {
  const sIdx = cellIdx(start.x, start.y, start.z);
  const tIdx = cellIdx(target.x, target.y, target.z);
  if (sIdx === tIdx) return 0;
  const dist = new Int16Array(N3).fill(-1) as Int16Array;
  dist[sIdx] = 0;
  const queue: number[] = [sIdx];
  let head = 0;
  while (head < queue.length) {
    const cur = queue[head++];
    const cx = cur % N;
    const cy = Math.floor(cur / N) % N;
    const cz = Math.floor(cur / (N * N));
    const cd = dist[cur];
    for (const [dx, dy, dz] of [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]] as const) {
      const nx = cx + dx, ny = cy + dy, nz = cz + dz;
      if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
      const ni = cellIdx(nx, ny, nz);
      if (dist[ni] !== -1) continue;
      if (ni === tIdx) return cd + 1;
      if (occ[ni]) continue;
      dist[ni] = cd + 1;
      queue.push(ni);
    }
  }
  return Infinity;
}

/**
 * Count how many empty cells are reachable from `start` (the start cell
 * itself counts as 1 even if technically empty). Used for "free space"
 * fallback scoring when no fully-safe move exists.
 */
function reachableCount(occ: Uint8Array, start: Segment): number {
  const sIdx = cellIdx(start.x, start.y, start.z);
  const seen = new Uint8Array(N3);
  seen[sIdx] = 1;
  let count = 1;
  const queue: number[] = [sIdx];
  let head = 0;
  while (head < queue.length) {
    const cur = queue[head++];
    const cx = cur % N;
    const cy = Math.floor(cur / N) % N;
    const cz = Math.floor(cur / (N * N));
    for (const [dx, dy, dz] of [[1,0,0],[-1,0,0],[0,1,0],[0,-1,0],[0,0,1],[0,0,-1]] as const) {
      const nx = cx + dx, ny = cy + dy, nz = cz + dz;
      if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;
      const ni = cellIdx(nx, ny, nz);
      if (seen[ni] || occ[ni]) continue;
      seen[ni] = 1;
      count++;
      queue.push(ni);
    }
  }
  return count;
}

interface AutoOption {
  dir: SnakeDir;
  newHead: Segment;
  willEat: boolean;
  /** Distance from the post-move head to the apple. Infinity = no path. */
  distToApple: number;
  /** True if from the post-move head we can still reach the new tail tip. */
  safe: boolean;
  /** Free-cell count reachable from new head (ignoring tail mobility). */
  space: number;
}

/**
 * Pick a direction for the snake. Strategy:
 *   1. Enumerate the legal moves (in-bounds, not reverse, no immediate body hit).
 *   2. For each move, simulate the resulting body and check whether the new
 *      head can still reach the new tail through empty cells. Moves passing
 *      this check are "safe" in the classical snake-AI sense — the snake
 *      can keep tail-following indefinitely if it has to.
 *   3. Prefer the safe move with the shortest path to the apple.
 *   4. With probability `nearMiss`, if the chosen move would land on the
 *      apple AND there is at least one safe non-eating alternative, swerve.
 *      Skips when no safe alternative exists so we don't kill the snake for
 *      drama.
 *   5. If no safe moves exist, pick the move that opens up the most
 *      reachable space (best chance of finding a way out next tick).
 */
function autoChoose(g: SnakeGame, nearMiss: number): SnakeDir {
  const head = g.segments[0];
  const apple = g.apple;
  const tail = g.segments[g.segments.length - 1];

  const options: AutoOption[] = [];
  for (const dir of ALL_DIRS) {
    if (dir === OPPOSITE[g.direction]) continue;
    const [dx, dy, dz] = STEP[dir];
    const nx = head.x + dx, ny = head.y + dy, nz = head.z + dz;
    if (nx < 0 || nx >= N || ny < 0 || ny >= N || nz < 0 || nz >= N) continue;

    const willEat = nx === apple.x && ny === apple.y && nz === apple.z;
    // Build the post-move body. If we eat, the old tail stays; if we don't,
    // the old tail tip moves out so its cell is free next tick.
    const newHead: Segment = { x: nx, y: ny, z: nz };
    const newBody: Segment[] = willEat
      ? [newHead, ...g.segments]
      : [newHead, ...g.segments.slice(0, -1)];

    // Reject moves that immediately collide with our own body. When eating,
    // the whole snake stays put — every cell is solid. When moving, the old
    // tail tip is freed.
    const hitsSelf = newBody.slice(1).some((s) => s.x === nx && s.y === ny && s.z === nz);
    if (hitsSelf) continue;

    const occWithoutHead = new Uint8Array(N3);
    for (let i = 1; i < newBody.length; i++) {
      occWithoutHead[cellIdx(newBody[i].x, newBody[i].y, newBody[i].z)] = 1;
    }

    // Distance from new head to the apple.
    const distToApple = willEat ? 0 : bfsDist(occWithoutHead, newHead, apple);

    // Safety: from the new head, can we reach the new tail tip through
    // empty cells? Treat the tail tip itself as a passable goal — its cell
    // will be free by the time the snake gets there since the body shifts
    // each tick. Use the body that excludes the new tail tip as obstacles.
    const newTail = newBody[newBody.length - 1];
    const occForTailCheck = new Uint8Array(N3);
    for (let i = 1; i < newBody.length - 1; i++) {
      occForTailCheck[cellIdx(newBody[i].x, newBody[i].y, newBody[i].z)] = 1;
    }
    const safe = bfsDist(occForTailCheck, newHead, newTail) !== Infinity;
    const space = reachableCount(occWithoutHead, newHead);

    options.push({ dir, newHead, willEat, distToApple, safe, space });
  }

  if (options.length === 0) {
    // Doomed — keep current heading and let the game end gracefully.
    return g.direction;
  }

  const safeOpts = options.filter((o) => o.safe);
  if (safeOpts.length > 0) {
    // Prefer reachable apple. Break ties by space — heading into an open
    // chamber tends to leave more options for the next move.
    safeOpts.sort((a, b) => {
      if (a.distToApple !== b.distToApple) return a.distToApple - b.distToApple;
      return b.space - a.space;
    });
    const top = safeOpts[0];

    // Near-miss: if the chosen move would eat AND there's a safe alternative
    // that doesn't eat, occasionally swerve. Pick the swerve that's still
    // close to the apple (drama, not stalling).
    if (top.willEat && nearMiss > 0 && Math.random() < nearMiss) {
      const swerves = safeOpts
        .filter((o) => !o.willEat && o.distToApple !== Infinity)
        .sort((a, b) => a.distToApple - b.distToApple);
      if (swerves.length > 0) return swerves[0].dir;
    }
    return top.dir;
  }

  // No safe moves — survival fallback. Pick the move that leaves the most
  // free space; among ties prefer one closer to the tail (which gives the
  // most room as the body shifts forward).
  options.sort((a, b) => {
    if (b.space !== a.space) return b.space - a.space;
    const da = Math.abs(a.newHead.x - tail.x) + Math.abs(a.newHead.y - tail.y) + Math.abs(a.newHead.z - tail.z);
    const db = Math.abs(b.newHead.x - tail.x) + Math.abs(b.newHead.y - tail.y) + Math.abs(b.newHead.z - tail.z);
    return da - db;
  });
  return options[0].dir;
}

