import { CUBE_N } from './types';

/**
 * A signed-permutation 3x3 matrix representing one of the 24 cube orientations.
 * Applied to (x,y,z) about the cube center, all entries stay integer 0..N-1.
 */
export type Orientation = number[][];

export const identityOrient: Orientation = [
  [1, 0, 0],
  [0, 1, 0],
  [0, 0, 1],
];

const ROT_X: Orientation = [
  [1, 0, 0],
  [0, 0, -1],
  [0, 1, 0],
];
const ROT_Y: Orientation = [
  [0, 0, 1],
  [0, 1, 0],
  [-1, 0, 0],
];
const ROT_Z: Orientation = [
  [0, -1, 0],
  [1, 0, 0],
  [0, 0, 1],
];

export function compose(a: Orientation, b: Orientation): Orientation {
  const out: Orientation = [
    [0, 0, 0],
    [0, 0, 0],
    [0, 0, 0],
  ];
  for (let i = 0; i < 3; i++) {
    for (let j = 0; j < 3; j++) {
      let s = 0;
      for (let k = 0; k < 3; k++) s += a[i][k] * b[k][j];
      out[i][j] = s;
    }
  }
  return out;
}

export const rotateX = (o: Orientation): Orientation => compose(ROT_X, o);
export const rotateY = (o: Orientation): Orientation => compose(ROT_Y, o);
export const rotateZ = (o: Orientation): Orientation => compose(ROT_Z, o);

const C = (CUBE_N - 1) / 2;

/** Apply orientation to (x,y,z), keeping the result integer in [0, N-1]. */
export function apply(o: Orientation, x: number, y: number, z: number): [number, number, number] {
  const dx = x - C;
  const dy = y - C;
  const dz = z - C;
  const ox = o[0][0] * dx + o[0][1] * dy + o[0][2] * dz + C;
  const oy = o[1][0] * dx + o[1][1] * dy + o[1][2] * dz + C;
  const oz = o[2][0] * dx + o[2][1] * dy + o[2][2] * dz + C;
  return [Math.round(ox), Math.round(oy), Math.round(oz)];
}

export function isIdentity(o: Orientation): boolean {
  for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) {
    if (o[i][j] !== identityOrient[i][j]) return false;
  }
  return true;
}
