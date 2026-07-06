import { useEffect, useRef, useState } from 'react';
import { connect } from './lib/ws';
import { Controls } from './components/Controls';
import { Calibration } from './components/Calibration';
import { CubePreview } from './components/CubePreview';
import { OrientationControls } from './components/OrientationControls';
import { MicControl } from './components/MicControl';
import { ServerControls } from './components/ServerControls';
import { PreviewBackground } from './components/PreviewBackground';
import { PreviewUpAxis, type UpAxis } from './components/PreviewUpAxis';
import { SnakeHint } from './components/SnakeHint';
import { useSnakeKeys, type SnakeAxisFlips } from './lib/snakeKeys';
import { useMicrophone } from './lib/audio';
import {
  defaultAutoCycle,
  defaultLayout,
  NUM_LEDS,
  type AutoCycleSettings,
  type CalibrationSample,
  type ClientToServer,
  type Layout,
  type Orientation,
  type PatternMeta,
  type PatternParams,
  type SolveResult,
} from '@shared/types';
import { identityOrient } from '@shared/orientation';

type Tab = 'patterns' | 'calibrate';

interface CalibrationView {
  active: boolean;
  litLedIdx: number | null;
  samples: CalibrationSample[];
  solve: SolveResult | null;
}

const emptyCalibration: CalibrationView = {
  active: false,
  litLedIdx: null,
  samples: [],
  solve: null,
};

export function App() {
  const [tab, setTab] = useState<Tab>('patterns');
  const [patterns, setPatterns] = useState<PatternMeta[]>([]);
  const [patternId, setPatternId] = useState<string>('');
  const [params, setParams] = useState<PatternParams>({});
  const [layout, setLayout] = useState<Layout>(defaultLayout);
  const [orientation, setOrientation] = useState<Orientation>(identityOrient);
  const [fps, setFps] = useState(30);
  const [running, setRunning] = useState(true);
  const [brightness, setBrightness] = useState(1);
  const [gcIntervalSec, setGcIntervalSec] = useState(60);
  const [presetNames, setPresetNames] = useState<string[]>([]);
  const [autoCycle, setAutoCycle] = useState<AutoCycleSettings>(defaultAutoCycle);
  const [protocol, setProtocol] = useState<'dnrgb' | 'ddp'>('dnrgb');
  const [pacing, setPacing] = useState({ sendMsAvg: 0, sendMsMax: 0, jitterMsMax: 0 });
  const [wled, setWled] = useState({
    host: 'cube.local',
    resolvedAddr: null as string | null,
    sendOk: false,
    lastError: null as string | null,
    lastSendAgoMs: null as number | null,
    infoOk: false,
    infoAgoMs: null as number | null,
    wledVersion: null as string | null,
    ledCount: null as number | null,
  });
  const [previewBg, setPreviewBg] = useState<string>(() => {
    return localStorage.getItem('cube-light:previewBg') || '#0a0a0c';
  });
  const [previewUp, setPreviewUp] = useState<UpAxis>(() => {
    const v = localStorage.getItem('cube-light:previewUp');
    return v === 'x' || v === 'y' || v === 'z' ? v : 'z';
  });
  const [snakeFlips, setSnakeFlips] = useState<SnakeAxisFlips>(() => {
    try {
      const s = localStorage.getItem('cube-light:snakeFlips');
      if (s) {
        const parsed = JSON.parse(s) as Partial<SnakeAxisFlips>;
        return {
          x: Boolean(parsed.x),
          y: Boolean(parsed.y),
          z: Boolean(parsed.z),
        };
      }
    } catch {}
    return { x: false, y: false, z: false };
  });
  const [rgb, setRgb] = useState<Uint8Array | null>(null);
  const [calibration, setCalibration] = useState<CalibrationView>(emptyCalibration);
  const sendRef = useRef<((m: ClientToServer) => void) | null>(null);
  // Auto-cycle is server-side state, but we mirror it to localStorage so a
  // page reload restores the user's last setting. The ref gates the mirror:
  // we only start saving back after the first state arrives, so we don't
  // overwrite localStorage with the server's default before it's seen ours.
  const autoCycleHydratedRef = useRef(false);

  useEffect(() => {
    const conn = connect((msg) => {
      if (msg.type === 'state') {
        setPatterns(msg.patterns);
        setPatternId(msg.patternId);
        setParams(msg.params);
        setLayout(msg.layout);
        setOrientation(msg.orientation);
        setFps(msg.fps);
        setRunning(msg.running);
        setBrightness(msg.brightness);
        setGcIntervalSec(msg.gcIntervalSec);
        setPresetNames(msg.presetNames);
        setAutoCycle(msg.autoCycle);
        // On the very first state message, push localStorage's autoCycle
        // back to the server so its config wins over the server default.
        if (!autoCycleHydratedRef.current) {
          autoCycleHydratedRef.current = true;
          const stored = localStorage.getItem('cube-light:autoCycle');
          if (stored) {
            try {
              const parsed = JSON.parse(stored) as AutoCycleSettings;
              conn.send({ type: 'setAutoCycle', settings: parsed });
            } catch {
              // Stale or hand-edited; just ignore and keep the server default.
            }
          }
        }
        setProtocol(msg.protocol);
        setPacing(msg.pacing);
        setWled(msg.wled);
        setCalibration({
          active: msg.calibration.active,
          litLedIdx: msg.calibration.litLedIdx,
          samples: msg.calibration.samples,
          solve: msg.calibration.solve,
        });
      } else if (msg.type === 'frame') {
        // The wire format is binary now; rgb is already a fresh Uint8Array
        // backed by the WS message buffer. Take a copy if the renderer needs
        // stable backing storage across frames.
        setRgb(msg.rgb);
      }
    });
    sendRef.current = conn.send;
    return () => conn.close();
  }, []);

  // Mirror auto-cycle settings to localStorage on every server update. Skipped
  // until the first state has been received and our hydration push is sent;
  // otherwise the server's default would clobber the saved value on reload.
  useEffect(() => {
    if (!autoCycleHydratedRef.current) return;
    localStorage.setItem('cube-light:autoCycle', JSON.stringify(autoCycle));
  }, [autoCycle]);

  const send = (m: ClientToServer) => sendRef.current?.(m);
  const mic = useMicrophone(send);
  const snakeActive = patternId === 'snake-3d';
  useSnakeKeys(snakeActive, send, snakeFlips);

  return (
    <div style={{ display: 'flex', height: '100vh' }}>
      <aside style={panel}>
        <h2 style={h2}>Cube Light</h2>
        <div style={tabBar}>
          <button
            style={{ ...tabBtn, ...(tab === 'patterns' ? tabActive : null) }}
            onClick={() => setTab('patterns')}
          >
            Patterns
          </button>
          <button
            style={{ ...tabBtn, ...(tab === 'calibrate' ? tabActive : null) }}
            onClick={() => setTab('calibrate')}
          >
            Calibrate
            {calibration.active && <span style={dot} />}
          </button>
        </div>
        {tab === 'patterns' ? (
          <>
            <Controls
              patterns={patterns}
              patternId={patternId}
              params={params}
              fps={fps}
              running={running}
              send={send}
            />
            <MicControl
              active={mic.state.active}
              level={mic.state.level}
              bands={mic.state.bands}
              beat={mic.state.beat}
              error={mic.state.error}
              onStart={mic.start}
              onStop={mic.stop}
            />
            <ServerControls
              brightness={brightness}
              gcIntervalSec={gcIntervalSec}
              presetNames={presetNames}
              autoCycle={autoCycle}
              protocol={protocol}
              pacing={pacing}
              patterns={patterns}
              wled={wled}
              send={send}
            />
          </>
        ) : (
          <Calibration
            active={calibration.active}
            litLedIdx={calibration.litLedIdx}
            samples={calibration.samples}
            solve={calibration.solve}
            layout={layout}
            send={send}
          />
        )}
      </aside>
      <div style={{ flex: 1, position: 'relative' }}>
        <CubePreview rgb={rgb} layout={layout} background={previewBg} up={previewUp} />
        <OrientationControls orientation={orientation} send={send} />
        <PreviewBackground
          value={previewBg}
          onChange={(c) => {
            setPreviewBg(c);
            localStorage.setItem('cube-light:previewBg', c);
          }}
        />
        <PreviewUpAxis
          value={previewUp}
          onChange={(a) => {
            setPreviewUp(a);
            localStorage.setItem('cube-light:previewUp', a);
          }}
        />
        {snakeActive && (
          <SnakeHint
            auto={String(params.mode ?? '') === 'auto'}
            flips={snakeFlips}
            onFlipsChange={(f) => {
              setSnakeFlips(f);
              localStorage.setItem('cube-light:snakeFlips', JSON.stringify(f));
            }}
          />
        )}
      </div>
    </div>
  );
}

const panel: React.CSSProperties = {
  width: 360,
  padding: 16,
  background: '#15151a',
  borderRight: '1px solid #222',
  overflowY: 'auto',
  height: '100%',
  boxSizing: 'border-box',
};
const h2: React.CSSProperties = { margin: '0 0 12px', fontSize: 16 };
const tabBar: React.CSSProperties = {
  display: 'flex',
  gap: 4,
  marginBottom: 12,
  borderBottom: '1px solid #222',
};
const tabBtn: React.CSSProperties = {
  flex: 1,
  background: 'transparent',
  border: 'none',
  borderBottom: '2px solid transparent',
  color: '#888',
  padding: '8px 4px',
  cursor: 'pointer',
  fontSize: 13,
  display: 'inline-flex',
  alignItems: 'center',
  justifyContent: 'center',
  gap: 6,
};
const tabActive: React.CSSProperties = { color: '#eee', borderBottom: '2px solid #3a6ec5' };
const dot: React.CSSProperties = {
  display: 'inline-block',
  width: 6,
  height: 6,
  borderRadius: '50%',
  background: '#f5a524',
};
