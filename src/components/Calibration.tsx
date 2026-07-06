import { useEffect, useMemo, useState } from 'react';
import {
  CUBE_N,
  NUM_LEDS,
  type CalibrationSample,
  type ClientToServer,
  type Layout,
  type SolveResult,
} from '@shared/types';
import { describeLayout, formatSamples, nextSuggestedLed } from '@shared/calibration';

interface Props {
  active: boolean;
  litLedIdx: number | null;
  samples: CalibrationSample[];
  solve: SolveResult | null;
  layout: Layout;
  send: (m: ClientToServer) => void;
}

export function Calibration(p: Props) {
  const [coordX, setCoordX] = useState('');
  const [coordY, setCoordY] = useState('');
  const [coordZ, setCoordZ] = useState('');
  const [editing, setEditing] = useState(false);
  const [draftText, setDraftText] = useState('');

  // When the lit LED changes, reset the input fields.
  useEffect(() => {
    setCoordX('');
    setCoordY('');
    setCoordZ('');
  }, [p.litLedIdx]);

  const samplesText = useMemo(() => formatSamples(p.samples), [p.samples]);
  // Keep the text-mode draft in sync when not actively editing.
  useEffect(() => {
    if (!editing) setDraftText(samplesText);
  }, [samplesText, editing]);

  const lit = p.litLedIdx ?? 0;
  const suggestion = nextSuggestedLed(p.samples);

  const start = () => p.send({ type: 'calibrationActive', active: true });
  const stop = () => p.send({ type: 'calibrationActive', active: false });
  const setLed = (n: number) => p.send({ type: 'calibrationSetLitLed', ledIndex: clamp(n, 0, NUM_LEDS - 1) });
  const submit = () => {
    const x = Number(coordX), y = Number(coordY), z = Number(coordZ);
    if (![x, y, z].every((v) => Number.isInteger(v) && v >= 0 && v < CUBE_N)) {
      alert(`Each coordinate must be an integer 0..${CUBE_N - 1}.`);
      return;
    }
    p.send({ type: 'calibrationAddSample', sample: { ledIndex: lit, x, y, z } });
    // Auto-advance to the suggested next LED, or +1 if we're already done.
    const next = nextSuggestedLed([...p.samples.filter((s) => s.ledIndex !== lit), { ledIndex: lit, x, y, z }]);
    if (next != null) setLed(next);
    else setLed((lit + 1) % NUM_LEDS);
  };

  const apply = () => {
    if (p.solve && p.solve.candidates.length === 1) {
      p.send({ type: 'calibrationApply', layout: p.solve.candidates[0] });
    }
  };

  const r = p.solve;
  const nCand = r ? r.candidates.length : 0;

  return (
    <div>
      <div style={{ display: 'flex', gap: 8, marginBottom: 12 }}>
        {!p.active ? (
          <button style={primaryBtn} onClick={start}>Enter calibration mode</button>
        ) : (
          <button style={btn} onClick={stop}>Exit calibration</button>
        )}
      </div>
      {!p.active && (
        <p style={hint}>
          Calibration mode lights one LED at a time on the physical cube. You tell me the
          (x,y,z) coordinate of that pixel — pick any corner you want as your origin, just
          stay consistent. Five well-placed samples typically pin the wiring down.
        </p>
      )}

      {p.active && (
        <>
          <h3 style={h3}>Currently lit</h3>
          <div style={litRow}>
            <button style={smallBtn} onClick={() => setLed(lit - 1)} disabled={lit === 0}>−</button>
            <input
              style={{ ...numInput, width: 90, textAlign: 'center' }}
              type="number"
              min={0}
              max={NUM_LEDS - 1}
              value={lit}
              onChange={(e) => setLed(Number(e.target.value))}
            />
            <button style={smallBtn} onClick={() => setLed(lit + 1)} disabled={lit >= NUM_LEDS - 1}>+</button>
            {suggestion != null && suggestion !== lit && (
              <button style={ghostBtn} onClick={() => setLed(suggestion)}>
                Jump to suggested #{suggestion}
              </button>
            )}
          </div>

          <h3 style={h3}>Where is this pixel?</h3>
          <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 6 }}>
            <Coord label="x" value={coordX} setValue={setCoordX} />
            <Coord label="y" value={coordY} setValue={setCoordY} />
            <Coord label="z" value={coordZ} setValue={setCoordZ} />
          </div>
          <button style={{ ...primaryBtn, marginTop: 8 }} onClick={submit}>Log sample → next pixel</button>
        </>
      )}

      <h3 style={h3}>Samples ({p.samples.length})</h3>
      {!editing ? (
        <>
          <div style={listBox}>
            {p.samples.length === 0 ? (
              <p style={{ ...hint, margin: 8 }}>No samples yet.</p>
            ) : (
              p.samples.map((s) => (
                <div key={s.ledIndex} style={listRow}>
                  <span style={{ flex: 1 }}>
                    LED <strong>#{s.ledIndex}</strong> → ({s.x}, {s.y}, {s.z})
                  </span>
                  <button style={tinyBtn} onClick={() => setLed(s.ledIndex)}>light</button>
                  <button
                    style={{ ...tinyBtn, color: '#f88' }}
                    onClick={() => p.send({ type: 'calibrationDeleteSample', ledIndex: s.ledIndex })}
                  >
                    delete
                  </button>
                </div>
              ))
            )}
          </div>
          <div style={{ display: 'flex', gap: 6, marginTop: 6 }}>
            <button style={btn} onClick={() => setEditing(true)}>Edit as text</button>
            <button style={btn} onClick={() => navigator.clipboard.writeText(samplesText)}>Copy</button>
          </div>
        </>
      ) : (
        <>
          <textarea
            value={draftText}
            onChange={(e) => setDraftText(e.target.value)}
            style={{ ...textarea, height: 200 }}
          />
          <div style={{ display: 'flex', gap: 6, marginTop: 6 }}>
            <button
              style={primaryBtn}
              onClick={() => {
                p.send({ type: 'calibrationSetSamples', samplesText: draftText });
                setEditing(false);
              }}
            >
              Save
            </button>
            <button
              style={btn}
              onClick={() => {
                setDraftText(samplesText);
                setEditing(false);
              }}
            >
              Cancel
            </button>
          </div>
        </>
      )}

      <h3 style={h3}>Auto-detect</h3>
      {p.samples.length === 0 ? (
        <p style={hint}>Log at least one sample to start solving.</p>
      ) : r && r.candidates.length === 0 ? (
        <div>
          <p style={{ ...hint, color: '#f88' }}>
            No layout in the search space matches every sample. Closest miss: {r.bestEffort?.mismatches}{' '}
            of {p.samples.length}.
          </p>
          <p style={hint}>
            Copy the sample text above and paste it back in chat — the auto-detect search space may
            need extending.
          </p>
        </div>
      ) : r && r.candidates.length === 1 ? (
        <div>
          <p style={{ ...hint, color: '#9f9' }}>Solved! {describeLayout(r.candidates[0])}</p>
          {!sameLayout(r.candidates[0], p.layout) && (
            <button style={primaryBtn} onClick={apply}>Apply this layout</button>
          )}
        </div>
      ) : (
        <div>
          <p style={hint}>
            {nCand} layouts still match. Suggested next LED:{' '}
            <strong>#{r?.suggestedNextLedIdx ?? '—'}</strong>.
          </p>
        </div>
      )}
    </div>
  );
}

function Coord({ label, value, setValue }: { label: string; value: string; setValue: (v: string) => void }) {
  return (
    <div>
      <label style={{ fontSize: 11, color: '#888' }}>{label}</label>
      <input
        type="number"
        min={0}
        max={CUBE_N - 1}
        value={value}
        onChange={(e) => setValue(e.target.value)}
        style={numInput}
      />
    </div>
  );
}

function clamp(n: number, lo: number, hi: number) {
  return Math.max(lo, Math.min(hi, Math.floor(n)));
}

function sameLayout(a: Layout, b: Layout): boolean {
  return (
    a.flipX === b.flipX &&
    a.flipY === b.flipY &&
    a.flipZ === b.flipZ &&
    a.ledOffset === b.ledOffset
  );
}

const input: React.CSSProperties = {
  width: '100%',
  background: '#222',
  color: '#eee',
  border: '1px solid #333',
  padding: 4,
  borderRadius: 4,
};
const btn: React.CSSProperties = { ...input, cursor: 'pointer' };
const primaryBtn: React.CSSProperties = { ...btn, background: '#264f8e', border: '1px solid #3a6ec5' };
const ghostBtn: React.CSSProperties = { ...btn, background: 'transparent', border: '1px solid #3a6ec5', color: '#aac6f3' };
const smallBtn: React.CSSProperties = {
  width: 32,
  height: 32,
  background: '#222',
  color: '#eee',
  border: '1px solid #333',
  borderRadius: 4,
  cursor: 'pointer',
};
const tinyBtn: React.CSSProperties = {
  background: 'transparent',
  border: 'none',
  color: '#aac6f3',
  cursor: 'pointer',
  fontSize: 11,
};
const numInput: React.CSSProperties = { ...input, padding: '6px 4px' };
const litRow: React.CSSProperties = { display: 'flex', gap: 6, alignItems: 'center', flexWrap: 'wrap' };
const listBox: React.CSSProperties = {
  border: '1px solid #2a2a2f',
  borderRadius: 4,
  maxHeight: 220,
  overflowY: 'auto',
  background: '#0e0e12',
};
const listRow: React.CSSProperties = {
  display: 'flex',
  alignItems: 'center',
  padding: '6px 8px',
  borderBottom: '1px solid #1c1c22',
  fontSize: 12,
  fontFamily: 'ui-monospace, monospace',
};
const textarea: React.CSSProperties = {
  width: '100%',
  background: '#0e0e12',
  color: '#ddd',
  border: '1px solid #2a2a2f',
  borderRadius: 4,
  fontFamily: 'ui-monospace, monospace',
  fontSize: 12,
  padding: 8,
  boxSizing: 'border-box',
};
const h3: React.CSSProperties = {
  margin: '16px 0 6px',
  fontSize: 12,
  color: '#888',
  textTransform: 'uppercase',
  letterSpacing: 1,
};
const hint: React.CSSProperties = { fontSize: 11, color: '#777', margin: '4px 0 8px' };
