import { CUBE_N, NUM_LEDS, type CalibrationSample, type Layout, type SolveResult } from './types';
import { makeIndex, makeInverse } from './geometry';

/**
 * Enumerate every Layout in the search space.
 * 8 flip combinations × NUM_LEDS offsets — but the offset is uniquely
 * determined by any single (LED, x, y, z) sample, so in practice the
 * solver short-circuits once it has one sample.
 */
export function enumerateLayouts(): Layout[] {
  const out: Layout[] = [];
  for (const fx of [false, true]) {
    for (const fy of [false, true]) {
      for (const fz of [false, true]) {
        for (let off = 0; off < NUM_LEDS; off++) {
          out.push({ flipX: fx, flipY: fy, flipZ: fz, ledOffset: off });
        }
      }
    }
  }
  return out;
}

export function layoutEquals(a: Layout, b: Layout): boolean {
  return (
    a.flipX === b.flipX &&
    a.flipY === b.flipY &&
    a.flipZ === b.flipZ &&
    a.ledOffset === b.ledOffset
  );
}

export function describeLayout(l: Layout): string {
  const flips = [l.flipX && 'X', l.flipY && 'Y', l.flipZ && 'Z'].filter(Boolean).join('') || 'none';
  return `flips=${flips} ledOffset=${l.ledOffset}`;
}

const SAMPLE_LINE_RE = /^\s*(\d+)\s*[, \t]\s*(\d+)\s*[, \t]\s*(\d+)\s*[, \t]\s*(\d+)\s*$/;

export function parseSamples(text: string): { samples: CalibrationSample[]; errors: string[] } {
  const samples: CalibrationSample[] = [];
  const errors: string[] = [];
  const seen = new Set<number>();
  const lines = text.split(/\r?\n/);
  lines.forEach((raw, i) => {
    const line = raw.trim();
    if (!line || line.startsWith('#')) return;
    const m = SAMPLE_LINE_RE.exec(line);
    if (!m) {
      errors.push(`line ${i + 1}: cannot parse "${raw}"`);
      return;
    }
    const ledIndex = Number(m[1]);
    const x = Number(m[2]);
    const y = Number(m[3]);
    const z = Number(m[4]);
    if (ledIndex < 0 || ledIndex >= NUM_LEDS) {
      errors.push(`line ${i + 1}: ledIndex ${ledIndex} out of range`);
      return;
    }
    if (
      x < 0 || x >= CUBE_N ||
      y < 0 || y >= CUBE_N ||
      z < 0 || z >= CUBE_N
    ) {
      errors.push(`line ${i + 1}: coord (${x},${y},${z}) out of 0..${CUBE_N - 1}`);
      return;
    }
    if (seen.has(ledIndex)) {
      errors.push(`line ${i + 1}: duplicate ledIndex ${ledIndex}`);
      return;
    }
    seen.add(ledIndex);
    samples.push({ ledIndex, x, y, z });
  });
  samples.sort((a, b) => a.ledIndex - b.ledIndex);
  return { samples, errors };
}

export function formatSamples(samples: CalibrationSample[]): string {
  const header = [
    '# cube-light calibration samples',
    '# format: ledIndex,x,y,z   (x,y,z each 0..9)',
    '# (0,0,0) = the corner of the cube you call the origin; pick once and stick with it',
  ];
  const sorted = [...samples].sort((a, b) => a.ledIndex - b.ledIndex);
  const body = sorted.map((s) => `${s.ledIndex},${s.x},${s.y},${s.z}`);
  return [...header, ...body, ''].join('\n');
}

/**
 * Find every layout consistent with the given samples.
 * Smart short-circuit: for each of 8 flip combos, the first sample uniquely
 * pins ledOffset; we then verify the rest. So solving is O(8 × samples).
 */
export function solve(samples: CalibrationSample[]): SolveResult {
  if (samples.length === 0) {
    return { candidates: [], suggestedNextLedIdx: 0 };
  }

  const candidates: Layout[] = [];
  let bestEffort: SolveResult['bestEffort'];

  for (const fx of [false, true]) {
    for (const fy of [false, true]) {
      for (const fz of [false, true]) {
        // For each flip combo, derive ledOffset from the first sample and check the rest.
        const trial: Layout = { flipX: fx, flipY: fy, flipZ: fz, ledOffset: 0 };
        const idxFn0 = makeIndex(trial);
        const s0 = samples[0];
        // makeIndex with offset=0 returns a "wire-position-derived index". The
        // offset that makes that equal s0.ledIndex is (wirePosIdx - s0.ledIndex) mod NUM_LEDS.
        const off = (idxFn0(s0.x, s0.y, s0.z) - s0.ledIndex + NUM_LEDS) % NUM_LEDS;
        const candidate: Layout = { flipX: fx, flipY: fy, flipZ: fz, ledOffset: off };
        const idxFn = makeIndex(candidate);
        let miss = 0;
        for (const s of samples) {
          if (idxFn(s.x, s.y, s.z) !== s.ledIndex) miss++;
        }
        if (miss === 0) candidates.push(candidate);
        if (!bestEffort || miss < bestEffort.mismatches) bestEffort = { layout: candidate, mismatches: miss };
      }
    }
  }

  if (candidates.length === 0) {
    return { candidates: [], suggestedNextLedIdx: null, bestEffort };
  }
  if (candidates.length === 1) {
    return { candidates, suggestedNextLedIdx: null };
  }

  return { candidates, suggestedNextLedIdx: chooseDisambiguatingLed(candidates, samples) };
}

function chooseDisambiguatingLed(candidates: Layout[], samples: CalibrationSample[]): number | null {
  const taken = new Set(samples.map((s) => s.ledIndex));
  const inverses = candidates.map((l) => makeInverse(l));
  let bestLed = -1;
  let bestScore = -1;
  for (let led = 0; led < NUM_LEDS; led++) {
    if (taken.has(led)) continue;
    const seen = new Set<string>();
    for (const inv of inverses) {
      const p = inv(led);
      if (!p) continue;
      seen.add(`${p.x},${p.y},${p.z}`);
    }
    const score = seen.size;
    if (score > bestScore) {
      bestScore = score;
      bestLed = led;
      if (score === candidates.length) break;
    }
  }
  return bestLed >= 0 ? bestLed : null;
}

export function nextSuggestedLed(samples: CalibrationSample[]): number | null {
  if (samples.length === 0) return 0;
  const r = solve(samples);
  if (r.candidates.length === 1) return null;
  return r.suggestedNextLedIdx;
}
