import { useEffect } from 'react';
import type { ClientToServer } from '@shared/types';

type Dir = 'x+' | 'x-' | 'y+' | 'y-' | 'z+' | 'z-';

export interface SnakeAxisFlips {
  x: boolean;
  y: boolean;
  z: boolean;
}

const KEY_MAP: Record<string, Dir> = {
  ArrowRight: 'x+',
  ArrowLeft: 'x-',
  KeyD: 'x+',
  KeyA: 'x-',
  ArrowUp: 'y+',
  ArrowDown: 'y-',
  KeyW: 'z+',
  KeyS: 'z-',
};

/**
 * Listen for snake-game keystrokes only when the snake pattern is active.
 * Arrow keys map to xy; W / S to z. `flips` inverts each axis at the input
 * boundary so the user can re-key the controls to match where they're sitting
 * relative to the cube.
 */
export function useSnakeKeys(
  active: boolean,
  send: (m: ClientToServer) => void,
  flips: SnakeAxisFlips,
) {
  useEffect(() => {
    if (!active) return;
    const onKey = (e: KeyboardEvent) => {
      // Don't hijack typing inside text fields.
      const target = e.target as HTMLElement;
      const tag = target?.tagName;
      if (tag === 'INPUT' || tag === 'TEXTAREA' || target?.isContentEditable) return;
      const dir = KEY_MAP[e.code];
      if (!dir) return;
      e.preventDefault();
      const axis = dir[0] as 'x' | 'y' | 'z';
      const sign = dir[1] as '+' | '-';
      const out = (flips[axis] ? (sign === '+' ? `${axis}-` : `${axis}+`) : dir) as Dir;
      send({ type: 'snakeInput', dir: out });
    };
    window.addEventListener('keydown', onKey);
    return () => window.removeEventListener('keydown', onKey);
  }, [active, send, flips.x, flips.y, flips.z]);
}
