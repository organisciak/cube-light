import { CUBE_N, NUM_LEDS, type Layout } from './types';

/**
 * Build (x,y,z) → LED-index for the cube's wiring.
 *
 * Wiring model (matches the physical cube on this project):
 *  - Vertical 10-LED strings along z.
 *  - Strings alternate direction by their GLOBAL string-index parity:
 *    even string → top-down, odd string → bottom-up.
 *  - 10 strings per row (along x). Rows alternate x-direction (forward,
 *    reverse, forward, …) so the wire continues end-to-end.
 *  - 10 rows stack along y.
 *  - The data wire's natural start lies somewhere along this path; LED 0 in
 *    WLED's numbering may sit anywhere along the snake. `ledOffset` shifts:
 *      LED N ↔ wire-position (N + ledOffset) mod NUM_LEDS.
 *  - flipX/Y/Z reorient the user's chosen logical (0,0,0) corner.
 */
export function makeIndex(layout: Layout): (x: number, y: number, z: number) => number {
  const N = CUBE_N;
  return (x, y, z) => {
    const xi = layout.flipX ? N - 1 - x : x;
    const yi = layout.flipY ? N - 1 - y : y;
    const zi = layout.flipZ ? N - 1 - z : z;
    const r = yi;                                  // row index (= y)
    const sir = r % 2 === 0 ? xi : N - 1 - xi;     // string-in-row, with row x-reversal
    const s = r * N + sir;                         // global string index, 0..99
    const p = s % 2 === 0 ? N - 1 - zi : zi;       // position within string: even→down, odd→up
    const wirePos = s * N + p;
    return ((wirePos - layout.ledOffset) % NUM_LEDS + NUM_LEDS) % NUM_LEDS;
  };
}

/**
 * Inverse: LED-index → (x, y, z) lookup table for the given layout.
 */
export function makeInverse(layout: Layout): (ledIdx: number) => { x: number; y: number; z: number } | null {
  const N = CUBE_N;
  const idxFn = makeIndex(layout);
  const table = new Array(NUM_LEDS) as ({ x: number; y: number; z: number } | null)[];
  for (let z = 0; z < N; z++) {
    for (let y = 0; y < N; y++) {
      for (let x = 0; x < N; x++) {
        const i = idxFn(x, y, z);
        table[i] = { x, y, z };
      }
    }
  }
  return (ledIdx) => (ledIdx >= 0 && ledIdx < table.length ? table[ledIdx] : null);
}

/** 3D coords for visualization, centered around origin in unit cube. */
export function ledPosition(x: number, y: number, z: number): [number, number, number] {
  const N = CUBE_N;
  const s = 1 / (N - 1);
  return [(x - (N - 1) / 2) * s, (y - (N - 1) / 2) * s, (z - (N - 1) / 2) * s];
}
