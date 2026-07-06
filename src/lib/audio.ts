import { useEffect, useRef, useState } from 'react';
import { AUDIO_BANDS, type ClientToServer } from '@shared/types';

interface MicState {
  active: boolean;
  level: number;
  bands: number[];
  /** Beat envelope (0..1), mirrors the server's onset detector for UI display. */
  beat: number;
  error: string | null;
}

const SEND_HZ = 30;

/**
 * useMicrophone — captures from the default audio input via getUserMedia,
 * runs an FFT in an AnalyserNode, computes log-spaced band magnitudes, and
 * forwards level + bands to the server at SEND_HZ. Patterns on the server
 * read these from PatternContext.audio.
 *
 * Returns current values for live UI display so we don't need a separate
 * state stream from the server.
 */
export function useMicrophone(send: (m: ClientToServer) => void): {
  state: MicState;
  start: () => Promise<void>;
  stop: () => void;
} {
  const [state, setState] = useState<MicState>({
    active: false,
    level: 0,
    bands: new Array(AUDIO_BANDS).fill(0),
    beat: 0,
    error: null,
  });

  // Beat detector mirrors the server's logic so the UI flashes in time with patterns.
  const beatHistRef = useRef<number[]>([]);
  const beatStampRef = useRef(0);
  const beatEnvRef = useRef(0);
  const lastTickRef = useRef(0);

  const ctxRef = useRef<AudioContext | null>(null);
  const streamRef = useRef<MediaStream | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const rafRef = useRef<number | null>(null);
  const lastSentRef = useRef(0);
  const sendRef = useRef(send);
  useEffect(() => {
    sendRef.current = send;
  }, [send]);

  const stop = () => {
    if (rafRef.current != null) cancelAnimationFrame(rafRef.current);
    rafRef.current = null;
    streamRef.current?.getTracks().forEach((t) => t.stop());
    streamRef.current = null;
    ctxRef.current?.close().catch(() => {});
    ctxRef.current = null;
    analyserRef.current = null;
    beatHistRef.current = [];
    beatEnvRef.current = 0;
    setState((s) => ({ ...s, active: false, level: 0, beat: 0, bands: new Array(AUDIO_BANDS).fill(0) }));
    // Push one zeroed frame so server forgets old audio immediately.
    sendRef.current?.({
      type: 'audioFrame',
      level: 0,
      bands: new Array(AUDIO_BANDS).fill(0),
    });
  };

  const start = async () => {
    if (streamRef.current) return;
    try {
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: false,
        },
      });
      streamRef.current = stream;
      const ctx = new AudioContext();
      ctxRef.current = ctx;
      const source = ctx.createMediaStreamSource(stream);
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 1024;
      analyser.smoothingTimeConstant = 0.6;
      source.connect(analyser);
      analyserRef.current = analyser;

      const fftBins = analyser.frequencyBinCount; // 512
      const freqData = new Uint8Array(fftBins);
      const timeData = new Uint8Array(analyser.fftSize);
      const sampleRate = ctx.sampleRate;
      const nyquist = sampleRate / 2;
      // Log-spaced band edges from ~30 Hz up to nyquist.
      const lo = 30;
      const hi = Math.min(nyquist, 14000);
      const bandEdges = new Array(AUDIO_BANDS + 1)
        .fill(0)
        .map((_, i) => lo * Math.pow(hi / lo, i / AUDIO_BANDS));
      const binPerHz = fftBins / nyquist;

      setState((s) => ({ ...s, active: true, error: null }));

      const tick = () => {
        analyser.getByteFrequencyData(freqData);
        analyser.getByteTimeDomainData(timeData);

        // Per-band average magnitude in 0..1.
        const bands = new Array(AUDIO_BANDS).fill(0).map((_, b) => {
          const startBin = Math.max(0, Math.floor(bandEdges[b] * binPerHz));
          const endBin = Math.min(fftBins - 1, Math.ceil(bandEdges[b + 1] * binPerHz));
          let sum = 0;
          let n = 0;
          for (let i = startBin; i <= endBin; i++) {
            sum += freqData[i];
            n++;
          }
          return n > 0 ? sum / n / 255 : 0;
        });

        // Time-domain RMS for overall level.
        let sumSq = 0;
        for (let i = 0; i < timeData.length; i++) {
          const v = (timeData[i] - 128) / 128;
          sumSq += v * v;
        }
        const rms = Math.sqrt(sumSq / timeData.length);
        const level = Math.min(1, rms * 2.5); // soft gain

        const tickNow = performance.now();
        const dt = lastTickRef.current ? (tickNow - lastTickRef.current) / 1000 : 0;
        lastTickRef.current = tickNow;

        // Mirror server-side beat detection for UI feedback.
        const bass = bands[0] ?? 0;
        beatHistRef.current.push(bass);
        if (beatHistRef.current.length > 30) beatHistRef.current.shift();
        beatEnvRef.current = Math.max(0, beatEnvRef.current - dt / 0.25);
        if (beatHistRef.current.length >= 5) {
          const avg = beatHistRef.current.reduce((s, v) => s + v, 0) / beatHistRef.current.length;
          if (
            bass > avg * 1.5 &&
            bass > 0.12 &&
            tickNow - beatStampRef.current > 150
          ) {
            beatStampRef.current = tickNow;
            beatEnvRef.current = 1;
          }
        }

        setState((s) => ({ ...s, level, bands, beat: beatEnvRef.current }));

        const now = performance.now();
        if (now - lastSentRef.current >= 1000 / SEND_HZ) {
          lastSentRef.current = now;
          sendRef.current?.({ type: 'audioFrame', level, bands });
        }

        rafRef.current = requestAnimationFrame(tick);
      };
      tick();
    } catch (e) {
      const error = (e as Error).message || String(e);
      setState((s) => ({ ...s, active: false, error }));
    }
  };

  useEffect(() => () => stop(), []); // eslint-disable-line react-hooks/exhaustive-deps

  return { state, start, stop };
}
