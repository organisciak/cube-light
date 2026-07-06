import type { AudioFrame, Layout, PatternMeta, PatternParams } from '../types';

export interface PatternContext {
  /** Frame buffer, length = NUM_LEDS * 3, RGB triples in 0..255. Mutate in place. */
  buffer: Uint8Array;
  /** Map (x,y,z) -> LED index for the current layout. */
  idx: (x: number, y: number, z: number) => number;
  /** Total elapsed seconds since pattern start. */
  t: number;
  /** Time since previous frame in seconds. */
  dt: number;
  /** Latest audio frame from the browser mic (zeros when no mic active). */
  audio: AudioFrame;
  /** Current pattern parameters. */
  params: PatternParams;
  /** Current layout for ergonomic access. */
  layout: Layout;
}

export interface Pattern {
  meta: PatternMeta;
  /** Called once when this pattern is selected. */
  init?: (ctx: PatternContext) => void;
  /** Called every frame. Should fill ctx.buffer. */
  render: (ctx: PatternContext) => void;
}
