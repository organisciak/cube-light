import express from 'express';
import { WebSocketServer } from 'ws';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { WledSender } from './wled.js';
import { DdpSender } from './ddp.js';
import { VirtualWled } from './virtualWled.js';
import {
  AUDIO_BANDS,
  NUM_LEDS,
  defaultAutoCycle,
  defaultLayout,
  emptyAudio,
  type AudioFrame,
  type AutoCycleSettings,
  type CalibrationSample,
  type ClientToServer,
  type Layout,
  type Orientation,
  type PatternMeta,
  type PatternParams,
  type ServerToClient,
  type SolveResult,
} from '../src/shared/types.js';
import { makeIndex } from '../src/shared/geometry.js';
import { calibrationPatternId, defaultPatternId, patterns } from '../src/shared/patterns/index.js';
import { queueSnakeInput } from '../src/shared/patterns/snake3d.js';
import { queuePacmanInput } from '../src/shared/patterns/pacman3d.js';
import type { PatternContext } from '../src/shared/patterns/types.js';
import { formatSamples, parseSamples, solve } from '../src/shared/calibration.js';
import { apply as applyOrient, identityOrient, rotateX, rotateY, rotateZ } from '../src/shared/orientation.js';

const PORT = Number(process.env.PORT ?? 3037);
const DEFAULT_WLED_HOST = process.env.WLED_HOST ?? 'cube.local';
const FALLBACK_HOST = '192.168.0.180';
const DATA_DIR = path.resolve(process.cwd(), 'data');
const CAL_FILE = path.join(DATA_DIR, 'calibration.csv');
const PRESETS_FILE = path.join(DATA_DIR, 'presets.json');
const WLED_HOST_FILE = path.join(DATA_DIR, 'wled-host.txt');

function loadWledHost(): string {
  try {
    const v = fs.readFileSync(WLED_HOST_FILE, 'utf8').trim();
    if (v) return v;
  } catch {}
  return DEFAULT_WLED_HOST;
}
function saveWledHost(host: string) {
  fs.mkdirSync(DATA_DIR, { recursive: true });
  fs.writeFileSync(WLED_HOST_FILE, host, 'utf8');
}

const WLED_HOST = loadWledHost();

const app = express();
app.use(express.json());
const httpServer = http.createServer(app);
const wss = new WebSocketServer({ server: httpServer, path: '/ws' });

const dnrgbSender = new WledSender({ host: WLED_HOST });
const ddpSender = new DdpSender(WLED_HOST);

interface SenderShim {
  send(buffer: Uint8Array): Promise<void>;
  resolve(): Promise<string>;
}

dnrgbSender
  .resolve()
  .catch(async () => {
    console.warn(`[wled] couldn't resolve ${WLED_HOST}, falling back to ${FALLBACK_HOST}`);
    dnrgbSender.setHost(FALLBACK_HOST);
    ddpSender.setHost(FALLBACK_HOST);
    await dnrgbSender.resolve().catch((e) => console.error('[wled] fallback also failed', e));
  })
  .then(() => {
    // Make sure WLED is on at the device level. If it was turned off (by a
    // preset, by saving "Turn on at boot=off", or by anyone hitting the power
    // button in the WLED UI), live-mode packets are silently swallowed until
    // we flip this. Best-effort — we don't fail startup if the POST errors.
    return dnrgbSender.seize();
  })
  .catch((e) => console.warn('[wled] seize failed', (e as Error).message));

function activeSender(): SenderShim {
  return state.protocol === 'ddp' ? (ddpSender as SenderShim) : (dnrgbSender as SenderShim);
}

// Convenience for places that called sender.send / sender.release directly.
const sender = {
  send: (b: Uint8Array) => activeSender().send(b),
  release: (restoreOn: boolean) => dnrgbSender.release(restoreOn),
};

interface CalibrationState {
  samples: CalibrationSample[];
  litLedIdx: number | null;
  active: boolean;
  solve: SolveResult | null;
}

interface RuntimeState {
  patternId: string;
  /** The pattern the user picked before entering calibration mode, restored on exit. */
  prevPatternId: string;
  params: PatternParams;
  layout: Layout;
  orientation: Orientation;
  fps: number;
  running: boolean;
  calibration: CalibrationState;
  audio: AudioFrame;
  /** When the last audio frame arrived. Stale audio decays to zero. */
  audioStamp: number;
  /** Seconds between GC sweeps (extra all-off frame). 0 disables. */
  gcIntervalSec: number;
  /** Final output brightness 0..1. */
  brightness: number;
  autoCycle: AutoCycleSettings;
  /** Beats accumulated since last auto-cycle advance. */
  autoCycleBeatCount: number;
  /** Wire protocol used to drive WLED. */
  protocol: 'dnrgb' | 'ddp';
}

interface Preset {
  name: string;
  patternId: string;
  params: PatternParams;
  layout: Layout;
  orientation: Orientation;
  brightness: number;
  gcIntervalSec: number;
  fps: number;
}

const state: RuntimeState = {
  patternId: defaultPatternId,
  prevPatternId: defaultPatternId,
  params: defaultParamsFor(defaultPatternId),
  layout: { ...defaultLayout },
  orientation: identityOrient.map((row) => [...row]),
  fps: 30,
  running: true,
  calibration: {
    samples: loadSamplesFromDisk(),
    litLedIdx: null,
    active: false,
    solve: null,
  },
  audio: { level: 0, bands: new Array(AUDIO_BANDS).fill(0), beat: 0 },
  audioStamp: 0,
  gcIntervalSec: 60,
  brightness: 1,
  autoCycle: { ...defaultAutoCycle },
  autoCycleBeatCount: 0,
  protocol: 'dnrgb' as 'dnrgb' | 'ddp',
};

// Connection telemetry: tracks whether WLED is actually receiving our packets
// and is reachable on its HTTP API.
const connState = {
  sendOk: false,
  lastError: null as string | null,
  lastSendStamp: 0,
  infoOk: false,
  infoStamp: 0,
  wledVersion: null as string | null,
  ledCount: null as number | null,
};

// Frame-pacing telemetry: rolling stats over the last STATS_WINDOW frames.
const STATS_WINDOW = 60;
const sendDurations: number[] = [];
const sendIntervals: number[] = [];
let lastSendStamp = 0;
function recordPacing(durMs: number) {
  sendDurations.push(durMs);
  if (sendDurations.length > STATS_WINDOW) sendDurations.shift();
  const now = performance.now();
  if (lastSendStamp > 0) {
    sendIntervals.push(now - lastSendStamp);
    if (sendIntervals.length > STATS_WINDOW) sendIntervals.shift();
  }
  lastSendStamp = now;
}
function pacingSnapshot() {
  if (sendDurations.length === 0) return { sendMsAvg: 0, sendMsMax: 0, jitterMsMax: 0 };
  const sendMsAvg = sendDurations.reduce((a, b) => a + b, 0) / sendDurations.length;
  const sendMsMax = sendDurations.reduce((a, b) => Math.max(a, b), 0);
  const expected = 1000 / state.fps;
  const jitterMsMax = sendIntervals.reduce(
    (a, b) => Math.max(a, Math.abs(b - expected)),
    0,
  );
  return {
    sendMsAvg: Math.round(sendMsAvg * 10) / 10,
    sendMsMax: Math.round(sendMsMax * 10) / 10,
    jitterMsMax: Math.round(jitterMsMax * 10) / 10,
  };
}

const DEFAULT_CYCLE_LIST = [
  'wavy-sheet',
  'rain',
  'rotating-planes',
  'audio-ripple',
  'fire',
  'life-3d',
];

let presets: Record<string, Preset> = loadPresetsFromDisk();

function loadPresetsFromDisk(): Record<string, Preset> {
  try {
    const txt = fs.readFileSync(PRESETS_FILE, 'utf8');
    const parsed = JSON.parse(txt) as Record<string, Preset>;
    console.log(`[presets] loaded ${Object.keys(parsed).length} presets`);
    return parsed;
  } catch {
    return {};
  }
}

function savePresetsToDisk() {
  fs.mkdirSync(DATA_DIR, { recursive: true });
  fs.writeFileSync(PRESETS_FILE, JSON.stringify(presets, null, 2), 'utf8');
}

function snapshotPreset(name: string): Preset {
  return {
    name,
    patternId: state.patternId,
    params: { ...state.params },
    layout: { ...state.layout },
    orientation: state.orientation.map((row) => [...row]),
    brightness: state.brightness,
    gcIntervalSec: state.gcIntervalSec,
    fps: state.fps,
  };
}

function applyPreset(p: Preset) {
  if (patterns[p.patternId]) {
    state.patternId = p.patternId;
    onPatternChanged();
  }
  state.params = { ...p.params };
  state.layout = { ...p.layout };
  state.orientation = p.orientation.map((row) => [...row]);
  state.brightness = clamp01(p.brightness);
  state.gcIntervalSec = Math.max(0, Math.floor(p.gcIntervalSec));
  state.fps = Math.max(1, Math.min(60, Math.floor(p.fps)));
  rebuildIndex();
  restartLoop();
}

// Beat detector state: rolling history of bass-band energy. The window is
// long enough to act as a stable noise floor (3s at 30Hz), and the noise
// floor average is computed from OLDER samples only — the most recent
// frames are excluded so a loud kick can't pull the average up to where it
// then fails its own threshold check. Combined with a rising-edge gate, this
// fires reliably on each kick instead of latching on sustained energy.
const BEAT_HISTORY_SIZE = 90;       // ~3s at 30Hz
const BEAT_FLOOR_EXCLUDE = 5;       // last ~150ms of samples skipped from floor avg
const BEAT_MIN_INTERVAL_MS = 180;
const BEAT_THRESHOLD_MULT = 1.3;    // current bass must exceed older-floor avg × this
const BEAT_MIN_LEVEL = 0.10;        // suppress detection in silence
const BEAT_MIN_RISE = 0.04;         // current must exceed previous frame by this
const BEAT_DECAY_S = 0.25;          // envelope decay
const beatState = {
  history: [] as number[],
  lastBeatStamp: 0,
  envelope: 0,
};
state.calibration.solve = solve(state.calibration.samples);

function defaultParamsFor(id: string): PatternParams {
  const p = patterns[id];
  if (!p) return {};
  const out: PatternParams = {};
  for (const spec of p.meta.params) out[spec.key] = spec.default;
  return out;
}

function loadSamplesFromDisk(): CalibrationSample[] {
  try {
    const txt = fs.readFileSync(CAL_FILE, 'utf8');
    const { samples, errors } = parseSamples(txt);
    if (errors.length) console.warn(`[calibration] parse warnings:\n${errors.join('\n')}`);
    console.log(`[calibration] loaded ${samples.length} samples from ${CAL_FILE}`);
    return samples;
  } catch {
    return [];
  }
}

function saveSamplesToDisk() {
  fs.mkdirSync(DATA_DIR, { recursive: true });
  fs.writeFileSync(CAL_FILE, formatSamples(state.calibration.samples), 'utf8');
}

const buffer = new Uint8Array(NUM_LEDS * 3);
const outBuffer = new Uint8Array(NUM_LEDS * 3);

/** Apply master brightness scaling into outBuffer; this is what gets sent + previewed. */
function projectOutput() {
  if (state.brightness >= 1) {
    outBuffer.set(buffer);
    return;
  }
  const b = state.brightness;
  for (let i = 0; i < buffer.length; i++) outBuffer[i] = Math.floor(buffer[i] * b);
}

let layoutIdx = makeIndex(state.layout);
let seq = 0;
let last = performance.now();
let patternStart = performance.now();
let initialized = false;

function rebuildIndex() {
  layoutIdx = makeIndex(state.layout);
}

/** Composed (x,y,z) -> ledIdx that applies orientation, then layout wiring. */
function composedIdx(x: number, y: number, z: number): number {
  const [px, py, pz] = applyOrient(state.orientation, x, y, z);
  return layoutIdx(px, py, pz);
}

function onPatternChanged() {
  patternStart = performance.now();
  initialized = false;
  state.params = defaultParamsFor(state.patternId);
}

function tickOnce() {
  const now = performance.now();
  const dt = (now - last) / 1000;
  last = now;
  const t = (now - patternStart) / 1000;
  const pat = patterns[state.patternId];
  if (!pat) return;
  // Decay audio to zero if no frame received in the last 250ms (mic disabled or stalled).
  // Decay beat envelope continuously so patterns get a smooth ramp-down.
  beatState.envelope = Math.max(0, beatState.envelope - dt / BEAT_DECAY_S);
  const audio: AudioFrame =
    now - state.audioStamp < 250
      ? { level: state.audio.level, bands: state.audio.bands, beat: beatState.envelope }
      : emptyAudio;
  const ctx: PatternContext = {
    buffer,
    idx: composedIdx,
    t,
    dt,
    audio,
    params: state.params,
    layout: state.layout,
  };
  if (!initialized && pat.init) {
    pat.init(ctx);
    initialized = true;
  }
  pat.render(ctx);
}

/**
 * Frames go out as a binary WS message: 5-byte header + 3000 bytes RGB.
 *   byte 0:    opcode (1 = frame)
 *   bytes 1-4: uint32 seq (BE)
 *   bytes 5+:  packed RGB triples
 * State and other control messages still use JSON. Client switches on
 * `typeof e.data` to dispatch.
 */
const FRAME_OPCODE = 1;
function broadcastFrame() {
  seq = (seq + 1) >>> 0;
  const out = new Uint8Array(5 + outBuffer.length);
  out[0] = FRAME_OPCODE;
  out[1] = (seq >>> 24) & 0xff;
  out[2] = (seq >>> 16) & 0xff;
  out[3] = (seq >>> 8) & 0xff;
  out[4] = seq & 0xff;
  out.set(outBuffer, 5);
  for (const ws of wss.clients) if (ws.readyState === ws.OPEN) ws.send(out);
}

/**
 * Force-send an all-off frame right now and broadcast to WS clients.
 * Use this at mode transitions (enter/exit calibration, pause) to guarantee
 * no residual pixels from the previous mode bleed through.
 *
 * Note: every regular frame already sends all 1000 LEDs, so this is mostly
 * a hygiene measure for transition moments and brief pauses where we'd
 * otherwise leave the cube showing the last frame until WLED's UDP timeout.
 */
async function blackout() {
  buffer.fill(0);
  outBuffer.fill(0);
  broadcastFrame();
  try {
    await sender.send(outBuffer);
  } catch (e) {
    console.error('[wled] blackout send failed', (e as Error).message);
  }
}

// With VIRTUAL_WLED=1, this server also *receives* DNRGB on udp/21324 and
// mirrors those frames to the preview instead of running its own patterns —
// used to preview the C++ firmware core (firmware/native) with no cube.
const virtualWled = process.env.VIRTUAL_WLED ? new VirtualWled() : null;
virtualWled?.start();

let timer: NodeJS.Timeout | null = null;
let lastGcStamp = 0;
function startLoop() {
  if (timer) return;
  const interval = Math.max(10, Math.floor(1000 / state.fps));
  timer = setInterval(async () => {
    if (virtualWled?.active()) {
      // Live frames are raw LED-order RGB; they bypass orientation and
      // brightness exactly as they would on real WLED hardware.
      outBuffer.set(virtualWled.buffer);
      broadcastFrame();
      return;
    }
    if (!state.running) return;
    tickOnce();
    projectOutput();
    broadcastFrame();
    if (virtualWled) return; // no real cube in virtual mode; preview only
    try {
      const now = performance.now();
      if (state.gcIntervalSec > 0 && now - lastGcStamp >= state.gcIntervalSec * 1000) {
        lastGcStamp = now;
        const zeros = new Uint8Array(outBuffer.length);
        await sender.send(zeros).catch(() => {});
      }
      const sendStart = performance.now();
      await sender.send(outBuffer);
      recordPacing(performance.now() - sendStart);
      connState.sendOk = true;
      connState.lastError = null;
      connState.lastSendStamp = performance.now();
    } catch (e) {
      const msg = (e as Error).message;
      connState.sendOk = false;
      connState.lastError = msg;
      console.error('[wled] send failed', msg);
    }
  }, interval);
}
function restartLoop() {
  if (timer) clearInterval(timer);
  timer = null;
  startLoop();
}
startLoop();

/**
 * Probe WLED's HTTP info endpoint. Confirms the device is reachable on its
 * web interface (independent of UDP send success) and surfaces firmware
 * version + configured LED count so the user can sanity-check the setup.
 */
async function probeWledInfo() {
  const host = dnrgbSender.getHost();
  const ctl = new AbortController();
  // WLED's HTTP is single-threaded behind the LED rendering loop; while it's
  // streaming live frames, /json/info can take 5+ seconds to respond. Use a
  // generous timeout so we don't false-positive "disconnected" mid-show.
  const timer = setTimeout(() => ctl.abort(), 8000);
  try {
    const res = await fetch(`http://${host}/json/info`, { signal: ctl.signal });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const j = (await res.json()) as { ver?: string; leds?: { count?: number } };
    const prevOk = connState.infoOk;
    const prevVer = connState.wledVersion;
    const prevCount = connState.ledCount;
    connState.infoOk = true;
    connState.infoStamp = performance.now();
    connState.wledVersion = j.ver ?? null;
    connState.ledCount = j.leds?.count ?? null;
    if (!prevOk || prevVer !== connState.wledVersion || prevCount !== connState.ledCount) {
      broadcastState();
    }
  } catch {
    const prevOk = connState.infoOk;
    connState.infoOk = false;
    connState.infoStamp = performance.now();
    if (prevOk) broadcastState();
  } finally {
    clearTimeout(timer);
  }
}
setInterval(() => { probeWledInfo().catch(() => {}); }, 3000);
probeWledInfo().catch(() => {});
// Periodically rebroadcast state so connection age (lastSendAgoMs / infoAgoMs)
// keeps ticking in the UI without the user having to interact.
setInterval(() => broadcastState(), 1000);

function snapshotState(): ServerToClient {
  const metas: PatternMeta[] = Object.values(patterns).map((p) => p.meta);
  return {
    type: 'state',
    patternId: state.patternId,
    params: state.params,
    layout: state.layout,
    orientation: state.orientation,
    fps: state.fps,
    running: state.running,
    patterns: metas,
    calibration: { ...state.calibration },
    gcIntervalSec: state.gcIntervalSec,
    brightness: state.brightness,
    presetNames: Object.keys(presets).sort(),
    autoCycle: { ...state.autoCycle },
    protocol: state.protocol,
    pacing: pacingSnapshot(),
    wled: {
      host: dnrgbSender.getHost(),
      resolvedAddr: (dnrgbSender as any).addr ?? null,
      sendOk: connState.sendOk,
      lastError: connState.lastError,
      lastSendAgoMs:
        connState.lastSendStamp > 0
          ? Math.round(performance.now() - connState.lastSendStamp)
          : null,
      infoOk: connState.infoOk,
      infoAgoMs:
        connState.infoStamp > 0
          ? Math.round(performance.now() - connState.infoStamp)
          : null,
      wledVersion: connState.wledVersion,
      ledCount: connState.ledCount,
    },
  };
}

wss.on('connection', (ws) => {
  ws.send(JSON.stringify(snapshotState()));
  ws.on('message', (raw) => {
    let msg: ClientToServer;
    try {
      msg = JSON.parse(raw.toString()) as ClientToServer;
    } catch {
      return;
    }
    handle(msg);
    broadcastState();
  });
});

function broadcastState() {
  const data = JSON.stringify(snapshotState());
  for (const ws of wss.clients) if (ws.readyState === ws.OPEN) ws.send(data);
}

function enterCalibration() {
  if (state.calibration.active) return;
  state.calibration.active = true;
  state.prevPatternId = state.patternId;
  state.patternId = calibrationPatternId;
  onPatternChanged();
  applyLitLed();
  // Wipe any residual frame from the previous pattern so the user only sees
  // the single calibration pixel.
  blackout().catch(() => {});
}

function exitCalibration() {
  if (!state.calibration.active) return;
  state.calibration.active = false;
  state.patternId = state.prevPatternId;
  onPatternChanged();
  blackout().catch(() => {});
}

function applyLitLed() {
  if (state.patternId !== calibrationPatternId) return;
  state.params.ledIdx = state.calibration.litLedIdx ?? 0;
}

function refreshSolve() {
  state.calibration.solve = solve(state.calibration.samples);
}

function handle(msg: ClientToServer) {
  switch (msg.type) {
    case 'selectPattern':
      if (patterns[msg.patternId]) {
        // Selecting a non-calibration pattern from the UI implicitly exits calibration mode.
        if (state.calibration.active && msg.patternId !== calibrationPatternId) {
          state.calibration.active = false;
        }
        state.patternId = msg.patternId;
        onPatternChanged();
      }
      return;
    case 'setParam':
      state.params[msg.key] = msg.value;
      return;
    case 'setLayout':
      state.layout = { ...state.layout, ...msg.layout };
      rebuildIndex();
      return;
    case 'setFps':
      state.fps = Math.max(1, Math.min(60, Math.floor(msg.fps)));
      restartLoop();
      return;
    case 'setRunning':
      state.running = msg.running;
      if (!state.running) blackout().catch(() => {});
      return;

    case 'calibrationActive':
      if (msg.active) enterCalibration();
      else exitCalibration();
      return;
    case 'calibrationSetLitLed':
      state.calibration.litLedIdx =
        msg.ledIndex == null ? null : Math.max(0, Math.min(NUM_LEDS - 1, Math.floor(msg.ledIndex)));
      applyLitLed();
      return;
    case 'calibrationSetSamples': {
      const { samples, errors } = parseSamples(msg.samplesText);
      if (errors.length) console.warn(`[calibration] parse warnings:\n${errors.join('\n')}`);
      state.calibration.samples = samples;
      saveSamplesToDisk();
      refreshSolve();
      return;
    }
    case 'calibrationAddSample': {
      const s = msg.sample;
      const filtered = state.calibration.samples.filter((x) => x.ledIndex !== s.ledIndex);
      filtered.push(s);
      filtered.sort((a, b) => a.ledIndex - b.ledIndex);
      state.calibration.samples = filtered;
      saveSamplesToDisk();
      refreshSolve();
      return;
    }
    case 'calibrationDeleteSample':
      state.calibration.samples = state.calibration.samples.filter((s) => s.ledIndex !== msg.ledIndex);
      saveSamplesToDisk();
      refreshSolve();
      return;
    case 'calibrationSolve':
      refreshSolve();
      return;
    case 'calibrationApply':
      state.layout = { ...msg.layout };
      rebuildIndex();
      return;

    case 'audioFrame': {
      const level = clamp01(msg.level);
      const incoming = Array.isArray(msg.bands) ? msg.bands : [];
      const bands = new Array(AUDIO_BANDS).fill(0).map((_, i) => clamp01(incoming[i] ?? 0));
      const now = performance.now();
      state.audio = { level, bands, beat: state.audio.beat };
      state.audioStamp = now;

      // Onset detection: bass must rise above the noise floor, clear a
      // minimum level, beat its own immediately-preceding sample, and clear
      // the debounce window. The noise floor is the average of the OLDER
      // samples in the history (most recent BEAT_FLOOR_EXCLUDE skipped), so
      // a loud kick doesn't pollute the threshold it has to clear.
      const bass = bands[0] ?? 0;
      const prevBass = beatState.history.length > 0
        ? beatState.history[beatState.history.length - 1]
        : 0;
      beatState.history.push(bass);
      if (beatState.history.length > BEAT_HISTORY_SIZE) beatState.history.shift();
      const floorWindow = beatState.history.length - BEAT_FLOOR_EXCLUDE;
      if (floorWindow >= 10) {
        let sum = 0;
        for (let i = 0; i < floorWindow; i++) sum += beatState.history[i];
        const avg = sum / floorWindow;
        const since = now - beatState.lastBeatStamp;
        if (
          bass > avg * BEAT_THRESHOLD_MULT &&
          bass - prevBass > BEAT_MIN_RISE &&
          bass > BEAT_MIN_LEVEL &&
          since > BEAT_MIN_INTERVAL_MS
        ) {
          beatState.lastBeatStamp = now;
          beatState.envelope = 1;

          // Auto-cycle: accumulate beats; advance when threshold reached.
          if (state.autoCycle.enabled) {
            state.autoCycleBeatCount++;
            if (state.autoCycleBeatCount >= state.autoCycle.everyNBeats) {
              state.autoCycleBeatCount = 0;
              advanceCyclePattern();
            }
          }
        }
      }
      return;
    }
    case 'setGcInterval':
      state.gcIntervalSec = Math.max(0, Math.floor(msg.seconds));
      lastGcStamp = performance.now();
      return;
    case 'setBrightness':
      state.brightness = clamp01(msg.value);
      return;
    case 'savePreset': {
      const name = msg.name.trim();
      if (!name) return;
      presets[name] = snapshotPreset(name);
      savePresetsToDisk();
      return;
    }
    case 'loadPreset': {
      const p = presets[msg.name];
      if (p) applyPreset(p);
      return;
    }
    case 'deletePreset':
      delete presets[msg.name];
      savePresetsToDisk();
      return;

    case 'setAutoCycle':
      state.autoCycle = {
        ...state.autoCycle,
        ...msg.settings,
        everyNBeats: Math.max(1, Math.min(64, Math.floor(msg.settings.everyNBeats ?? state.autoCycle.everyNBeats))),
      };
      state.autoCycleBeatCount = 0;
      return;
    case 'cycleAdvance':
      // Manual override: jump to the next pattern in the cycle list now.
      // Reset the beat counter so the next auto advance starts fresh from
      // here rather than firing right after on a stale count.
      state.autoCycleBeatCount = 0;
      advanceCyclePattern();
      return;
    case 'setWledHost': {
      const host = msg.host.trim();
      if (!host) return;
      dnrgbSender.setHost(host);
      ddpSender.setHost(host);
      saveWledHost(host);
      // Reset connection state so the UI reflects the change immediately;
      // the next send and the next probe tick will refresh it.
      connState.sendOk = false;
      connState.lastError = null;
      connState.infoOk = false;
      connState.wledVersion = null;
      connState.ledCount = null;
      // Re-resolve in the background; surface failures via lastError.
      dnrgbSender
        .resolve()
        .then(() => dnrgbSender.seize())
        .catch((e) => {
          connState.lastError = (e as Error).message;
        });
      probeWledInfo().catch(() => {});
      return;
    }
    case 'setProtocol':
      if (msg.protocol === 'dnrgb' || msg.protocol === 'ddp') {
        state.protocol = msg.protocol;
        // Reset pacing window on protocol change since timings will differ.
        sendDurations.length = 0;
        sendIntervals.length = 0;
      }
      return;

    case 'snakeInput':
      // Directional-input bus shared by snake and pacman; forward to whoever's active.
      if (state.patternId === 'snake-3d') queueSnakeInput(msg.dir);
      else if (state.patternId === 'pacman-3d') queuePacmanInput(msg.dir);
      return;
    case 'releaseWled':
      // Pause our loop, blackout, then ask WLED to take back control.
      state.running = false;
      blackout()
        .then(() => sender.release(msg.restoreOn))
        .catch((e) => console.error('[wled] release failed', (e as Error).message));
      return;

    case 'rotateOrientation':
      state.orientation =
        msg.axis === 'x' ? rotateX(state.orientation) :
        msg.axis === 'y' ? rotateY(state.orientation) :
        rotateZ(state.orientation);
      return;
    case 'resetOrientation':
      state.orientation = identityOrient.map((r) => [...r]);
      return;
  }
}

function advanceCyclePattern() {
  const list = state.autoCycle.patternList.length > 0 ? state.autoCycle.patternList : DEFAULT_CYCLE_LIST;
  const valid = list.filter((id) => patterns[id]);
  if (valid.length === 0) return;
  const i = valid.indexOf(state.patternId);
  const next = valid[(i + 1) % valid.length];
  if (next !== state.patternId) {
    // Carry the 'cycle' palette choice across pattern changes — it's the
    // user opting into ongoing color motion, so resetting it to whatever
    // each new pattern defaults to defeats the point. Any other palette
    // value still reverts to the new pattern's default.
    const prevPalette = String(state.params.palette ?? '');
    state.patternId = next;
    onPatternChanged();
    if (prevPalette === 'cycle') {
      const nextSpec = patterns[next].meta.params.find((p) => p.key === 'palette');
      if (nextSpec) state.params.palette = 'cycle';
    }
    broadcastState();
  }
}

function clamp01(n: unknown): number {
  const v = typeof n === 'number' && Number.isFinite(n) ? n : 0;
  return v < 0 ? 0 : v > 1 ? 1 : v;
}

app.get('/api/health', (_req, res) => {
  res.json({ ok: true, leds: NUM_LEDS, host: dnrgbSender.getHost(), state });
});

httpServer.listen(PORT, () => {
  console.log(`cube-light server on http://localhost:${PORT}  (ws path: /ws)`);
  console.log(`driving WLED at ${WLED_HOST}`);
  console.log(`calibration samples: ${state.calibration.samples.length}`);
});

// On graceful shutdown: blackout and release WLED so the cube doesn't sit
// frozen on the last frame for `timeoutSecs` before reverting.
let shuttingDown = false;
async function shutdown(signal: string) {
  if (shuttingDown) return;
  shuttingDown = true;
  console.log(`\n[shutdown] ${signal} → blackout + release`);
  if (timer) clearInterval(timer);
  timer = null;
  state.running = false;
  try {
    buffer.fill(0);
    await sender.send(buffer);
    await sender.release(false);
  } catch (e) {
    console.error('[shutdown] release failed', (e as Error).message);
  }
  process.exit(0);
}
process.on('SIGINT', () => shutdown('SIGINT'));
process.on('SIGTERM', () => shutdown('SIGTERM'));
