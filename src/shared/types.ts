export const CUBE_N = 10;
export const NUM_LEDS = CUBE_N * CUBE_N * CUBE_N;

export type RGB = [number, number, number];

export type Axis = 'x' | 'y' | 'z';

/**
 * The cube is wired as a single 1000-LED serpentine snake:
 * vertical 10-LED strings (along z), 10 strings per row (along x), 10 rows
 * stacked (along y). Strings alternate direction by their global string
 * index parity (even→top-down, odd→bottom-up). Rows alternate x-direction
 * end-to-end. The data wire enters at one corner, so LED 0 may not be at
 * the start of a string — `ledOffset` shifts the LED numbering by N LEDs
 * along the wire path. The remaining knobs (flipX/Y/Z) reorient the cube
 * if the user's chosen origin doesn't match the wire's natural starting
 * corner.
 */
export interface Layout {
  flipX: boolean;
  flipY: boolean;
  flipZ: boolean;
  /** LED N corresponds to wire-position (N + ledOffset) mod 1000. */
  ledOffset: number;
}

export const defaultLayout: Layout = {
  flipX: false,
  flipY: false,
  flipZ: false,
  ledOffset: 0,
};

export interface PatternParams {
  [key: string]: number | string | boolean;
}

export interface PatternMeta {
  id: string;
  name: string;
  description: string;
  params: PatternParamSpec[];
}

export interface PatternParamSpec {
  key: string;
  label: string;
  type: 'number' | 'color' | 'bool' | 'palette' | 'enum' | 'string';
  min?: number;
  max?: number;
  step?: number;
  default: number | string | boolean;
  /** For type 'enum': list of allowed string values. */
  options?: string[];
}

export interface CalibrationSample {
  ledIndex: number;
  x: number;
  y: number;
  z: number;
}

export type Orientation = number[][];

export interface AutoCycleSettings {
  enabled: boolean;
  /** Advance to next pattern after this many detected beats. */
  everyNBeats: number;
  /** Pattern IDs to cycle through. Empty = use built-in default list. */
  patternList: string[];
}

export const defaultAutoCycle: AutoCycleSettings = {
  enabled: false,
  everyNBeats: 8,
  patternList: [],
};

export const AUDIO_BANDS = 8;

export interface AudioFrame {
  /** Overall RMS-ish level, 0..1 (0 when no mic active). */
  level: number;
  /** Log-spaced frequency bands, AUDIO_BANDS entries, each 0..1. */
  bands: number[];
  /**
   * Beat envelope, 0..1. Spikes to 1 on detected onsets (low-band energy ramp)
   * and decays exponentially. Patterns can read this for instantaneous kicks
   * that are sharper than `level`.
   */
  beat: number;
}

export const emptyAudio: AudioFrame = {
  level: 0,
  bands: new Array(AUDIO_BANDS).fill(0),
  beat: 0,
};

export interface SolveResult {
  /** Layouts that match every sample exactly. */
  candidates: Layout[];
  /** A suggested LED index to sample next that would best disambiguate remaining candidates. */
  suggestedNextLedIdx: number | null;
  /** If candidates is empty, this is the layout with fewest mismatches and the count. */
  bestEffort?: { layout: Layout; mismatches: number };
}

export type ServerToClient =
  | { type: 'frame'; seq: number; rgb: Uint8Array }
  | {
      type: 'state';
      patternId: string;
      params: PatternParams;
      layout: Layout;
      fps: number;
      running: boolean;
      patterns: PatternMeta[];
      orientation: Orientation;
      calibration: {
        samples: CalibrationSample[];
        litLedIdx: number | null;
        active: boolean;
        solve: SolveResult | null;
      };
      /** Seconds between GC sweeps (extra all-off frame). 0 disables. */
      gcIntervalSec: number;
      /** Final output brightness scale 0..1. */
      brightness: number;
      /** Names of saved presets (full snapshot stored server-side). */
      presetNames: string[];
      autoCycle: AutoCycleSettings;
      protocol: 'dnrgb' | 'ddp';
      pacing: { sendMsAvg: number; sendMsMax: number; jitterMsMax: number };
      wled: {
        host: string;
        resolvedAddr: string | null;
        sendOk: boolean;
        lastError: string | null;
        lastSendAgoMs: number | null;
        infoOk: boolean;
        infoAgoMs: number | null;
        wledVersion: string | null;
        ledCount: number | null;
      };
    }
  | { type: 'log'; level: 'info' | 'warn' | 'error'; msg: string };

export type ClientToServer =
  | { type: 'selectPattern'; patternId: string }
  | { type: 'setParam'; key: string; value: number | string | boolean }
  | { type: 'setLayout'; layout: Partial<Layout> }
  | { type: 'setFps'; fps: number }
  | { type: 'setRunning'; running: boolean }
  // calibration
  | { type: 'calibrationActive'; active: boolean }
  | { type: 'calibrationSetLitLed'; ledIndex: number | null }
  | { type: 'calibrationSetSamples'; samplesText: string }
  | { type: 'calibrationAddSample'; sample: CalibrationSample }
  | { type: 'calibrationDeleteSample'; ledIndex: number }
  | { type: 'calibrationSolve' }
  | { type: 'calibrationApply'; layout: Layout }
  // orientation
  | { type: 'rotateOrientation'; axis: Axis }
  | { type: 'resetOrientation' }
  // audio
  | { type: 'audioFrame'; level: number; bands: number[] }
  // server settings
  | { type: 'setGcInterval'; seconds: number }
  | { type: 'setBrightness'; value: number }
  | { type: 'releaseWled'; restoreOn: boolean }
  // presets
  | { type: 'savePreset'; name: string }
  | { type: 'loadPreset'; name: string }
  | { type: 'deletePreset'; name: string }
  // auto-cycle on beat
  | { type: 'setAutoCycle'; settings: Partial<AutoCycleSettings> }
  | { type: 'cycleAdvance' }
  // protocol selection
  | { type: 'setProtocol'; protocol: 'dnrgb' | 'ddp' }
  // WLED host
  | { type: 'setWledHost'; host: string }
  // snake input
  | { type: 'snakeInput'; dir: 'x+' | 'x-' | 'y+' | 'y-' | 'z+' | 'z-' };
