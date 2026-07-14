import type { Pattern } from './types';
import { wavySheet } from './wavySheet';
import { solid } from './solid';
import { indexWalk } from './indexWalk';
import { litPixel } from './litPixel';
import { rain } from './rain';
import { rotatingPlanes } from './rotatingPlanes';
import { audioRipple } from './audioRipple';
import { spectrumDiscs } from './spectrumDiscs';
import { cloud } from './cloud';
import { comet } from './comet';
import { text3d } from './text3d';
import { fire } from './fire';
import { life3d } from './life3d';
import { snake3d } from './snake3d';
import { pacman3d } from './pacman3d';
import { fireworks } from './fireworks';
import { barEq } from './barEq';
import { spiral } from './spiral';
import { spinCube } from './spinCube';
import { bounce } from './bounce';
import { orbit } from './orbit';
import { scan } from './scan';

export const patterns: Record<string, Pattern> = {
  [wavySheet.meta.id]: wavySheet,
  [rain.meta.id]: rain,
  [rotatingPlanes.meta.id]: rotatingPlanes,
  [audioRipple.meta.id]: audioRipple,
  [spectrumDiscs.meta.id]: spectrumDiscs,
  [barEq.meta.id]: barEq,
  [spiral.meta.id]: spiral,
  [spinCube.meta.id]: spinCube,
  [bounce.meta.id]: bounce,
  [orbit.meta.id]: orbit,
  [scan.meta.id]: scan,
  [cloud.meta.id]: cloud,
  [comet.meta.id]: comet,
  [text3d.meta.id]: text3d,
  [fire.meta.id]: fire,
  [life3d.meta.id]: life3d,
  [snake3d.meta.id]: snake3d,
  [pacman3d.meta.id]: pacman3d,
  [fireworks.meta.id]: fireworks,
  [solid.meta.id]: solid,
  [indexWalk.meta.id]: indexWalk,
  [litPixel.meta.id]: litPixel,
};

export const defaultPatternId = wavySheet.meta.id;
export const calibrationPatternId = litPixel.meta.id;
