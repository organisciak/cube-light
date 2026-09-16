// Audio input for the simulator. Mirrors firmware/src/audio_capture.cpp:
// an FFT into 8 log-spaced bands with per-band noise floor + peak tracking,
// attack/release smoothing, an RMS level against a decaying peak, and the
// bass band handed to the firmware's own BeatDetector (via WASM) for the
// beat envelope and BPM. The "fake" source is the native harness's synthetic
// groove (firmware/native/main.cpp fakeAudio).
export const BANDS = 8;

const PEAK_DECAY = 0.9995, ATTACK = 0.55, RELEASE = 0.10;

export class MicAnalyzer {
  constructor() {
    this.bandPeak = new Float32Array(BANDS).fill(1);
    this.bandFloor = new Float32Array(BANDS).fill(1e9);
    this.bandOut = new Float32Array(BANDS);
    this.levelPeak = 0.01; this.levelOut = 0;
    this.bass = 0; this.ctx = null; this.analyser = null; this.stream = null;
  }
  // kind: 'mic' (getUserMedia) or 'tab' (getDisplayMedia with audio — the
  // clean digital feed of whatever a browser tab, or on some platforms the
  // whole system, is playing; no room, no mic colouration).
  async start(kind = 'mic') {
    if (kind === 'tab') {
      const s = await navigator.mediaDevices.getDisplayMedia({
        video: { width: 320, height: 180, frameRate: 1 },  // required by the API; dropped below
        audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false },
        preferCurrentTab: false, selfBrowserSurface: 'exclude', systemAudio: 'include',
      });
      s.getVideoTracks().forEach(t => { t.stop(); s.removeTrack(t); });
      if (!s.getAudioTracks().length) throw new Error('no audio was shared — pick a tab or screen and tick "Share audio"');
      s.getAudioTracks()[0].onended = () => this.onended?.();
      this.stream = s;
    } else {
      this.stream = await navigator.mediaDevices.getUserMedia({ audio: { echoCancellation: false, noiseSuppression: false, autoGainControl: false } });
    }
    this.ctx = new (window.AudioContext || window.webkitAudioContext)();
    const src = this.ctx.createMediaStreamSource(this.stream);
    this.analyser = this.ctx.createAnalyser();
    this.analyser.fftSize = 1024;           // 512 bins; at 48 kHz ≈ 47 Hz/bin (firmware: 43 Hz)
    this.analyser.smoothingTimeConstant = 0;
    src.connect(this.analyser);
    this.freq = new Float32Array(this.analyser.frequencyBinCount);
    this.wave = new Float32Array(this.analyser.fftSize);
  }
  stop() {
    this.stream?.getTracks().forEach(t => t.stop());
    this.ctx?.close(); this.ctx = null; this.analyser = null;
  }
  // Call once per render frame. Returns {level, bands, bass}.
  update() {
    const a = this.analyser; if (!a) return null;
    a.getFloatFrequencyData(this.freq);
    a.getFloatTimeDomainData(this.wave);
    let sumSq = 0;
    for (let i = 0; i < this.wave.length; i++) sumSq += this.wave[i] * this.wave[i];
    const rms = Math.sqrt(sumSq / this.wave.length) * 32768;  // firmware works in int16 units
    const knee = 30;  // squelch/2 in the firmware's units, scaled for browser mics
    const gate = Math.min(1, Math.max(0, (rms - knee) / (knee + 1e-3)));
    for (let b = 0; b < BANDS; b++) {
      const lo = 1 << b, hi = Math.min(this.freq.length, 1 << (b + 1));
      let acc = 0;
      for (let i = lo; i < hi; i++) acc += Math.pow(10, this.freq[i] / 20) * 32768;
      const mag = acc / (hi - lo);
      this.bandFloor[b] = mag < this.bandFloor[b] ? mag : this.bandFloor[b] * 1.002 + 0.05;
      this.bandPeak[b] = Math.max(mag, Math.max(this.bandPeak[b] * PEAK_DECAY, this.bandFloor[b] + 1));
      const norm = Math.min(1, Math.max(0, (mag - this.bandFloor[b]) / (this.bandPeak[b] - this.bandFloor[b] + 1e-3)));
      const target = norm * gate;
      if (b === 0) this.bass = target;
      this.bandOut[b] += (target - this.bandOut[b]) * (target > this.bandOut[b] ? ATTACK : RELEASE);
    }
    this.levelPeak = Math.max(rms, this.levelPeak * PEAK_DECAY);
    const lt = Math.min(1, rms / this.levelPeak) * gate;
    this.levelOut += (lt - this.levelOut) * (lt > this.levelOut ? ATTACK : RELEASE);
    return { level: this.levelOut, bands: this.bandOut, bass: this.bass };
  }
}

export function fakeAudio(t, out) {
  out.level = 0.35 + 0.3 * Math.sin(t * 0.7);
  for (let i = 0; i < BANDS; i++) out.bands[i] = 0.5 + 0.5 * Math.sin(t * (1.1 + i * 0.37) + i * 1.7);
  const beatPeriod = 0.5;
  const since = t % beatPeriod;
  out.beat = Math.exp(-since * 6.0);
  out.bpm = 120;
  return out;
}
