import type { ClientToServer, PatternMeta, PatternParams } from '@shared/types';
import { paletteNames, getPalette } from '@shared/palettes';

interface Props {
  patterns: PatternMeta[];
  patternId: string;
  params: PatternParams;
  fps: number;
  running: boolean;
  send: (m: ClientToServer) => void;
}

export function Controls(p: Props) {
  // Hide the calibration-only "single pixel" pattern from the picker.
  const visible = p.patterns.filter((m) => m.id !== 'lit-pixel');
  const meta = p.patterns.find((m) => m.id === p.patternId);
  return (
    <div>
      <Row label="Pattern">
        <select
          value={p.patternId}
          onChange={(e) => p.send({ type: 'selectPattern', patternId: e.target.value })}
          style={input}
        >
          {visible.map((m) => (
            <option key={m.id} value={m.id}>
              {m.name}
            </option>
          ))}
          {p.patternId === 'lit-pixel' && <option value="lit-pixel">Single Pixel (Calibration)</option>}
        </select>
      </Row>
      {meta && <p style={hint}>{meta.description}</p>}

      <Row label="Running">
        <button style={btn} onClick={() => p.send({ type: 'setRunning', running: !p.running })}>
          {p.running ? 'Pause' : 'Play'}
        </button>
      </Row>

      <Row label={`FPS: ${p.fps}`}>
        <input
          type="range"
          min={1}
          max={60}
          value={p.fps}
          onChange={(e) => p.send({ type: 'setFps', fps: Number(e.target.value) })}
          style={input}
        />
      </Row>

      <h3 style={h3}>Params</h3>
      {meta?.params.map((spec) => {
        const v = p.params[spec.key] ?? spec.default;
        if (spec.type === 'number') {
          return (
            <Row key={spec.key} label={`${spec.label}: ${typeof v === 'number' ? v.toFixed(2) : v}`}>
              <input
                type="range"
                min={spec.min ?? 0}
                max={spec.max ?? 1}
                step={spec.step ?? 0.01}
                value={Number(v)}
                onChange={(e) =>
                  p.send({ type: 'setParam', key: spec.key, value: Number(e.target.value) })
                }
                style={input}
              />
            </Row>
          );
        }
        if (spec.type === 'bool') {
          return (
            <Row key={spec.key} label={spec.label}>
              <input
                type="checkbox"
                checked={Boolean(v)}
                onChange={(e) => p.send({ type: 'setParam', key: spec.key, value: e.target.checked })}
              />
            </Row>
          );
        }
        if (spec.type === 'palette') {
          const current = String(v);
          return (
            <Row key={spec.key} label={spec.label}>
              <div style={{ display: 'flex', gap: 6, alignItems: 'center' }}>
                <PaletteSwatch name={current} />
                <select
                  value={current}
                  onChange={(e) => p.send({ type: 'setParam', key: spec.key, value: e.target.value })}
                  style={{ ...input, flex: 1 }}
                >
                  {paletteNames.map((n) => (
                    <option key={n} value={n}>
                      {n}
                    </option>
                  ))}
                </select>
              </div>
            </Row>
          );
        }
        if (spec.type === 'string') {
          return (
            <Row key={spec.key} label={spec.label}>
              <input
                type="text"
                value={String(v)}
                onChange={(e) => p.send({ type: 'setParam', key: spec.key, value: e.target.value })}
                style={{ ...input, fontFamily: 'ui-monospace, monospace' }}
              />
            </Row>
          );
        }
        if (spec.type === 'enum') {
          return (
            <Row key={spec.key} label={spec.label}>
              <select
                value={String(v)}
                onChange={(e) => p.send({ type: 'setParam', key: spec.key, value: e.target.value })}
                style={input}
              >
                {(spec.options ?? []).map((n) => (
                  <option key={n} value={n}>
                    {n}
                  </option>
                ))}
              </select>
            </Row>
          );
        }
        return null;
      })}
    </div>
  );
}

function PaletteSwatch({ name }: { name: string }) {
  // Render a tiny gradient sampled from the palette so the user can preview at a glance.
  const palette = getPalette(name);
  const stops = 8;
  const grad = Array.from({ length: stops }, (_, i) => {
    const [r, g, b] = palette(i / (stops - 1));
    return `rgb(${r}, ${g}, ${b}) ${(i / (stops - 1)) * 100}%`;
  }).join(', ');
  return (
    <div
      title={name}
      style={{
        width: 28,
        height: 22,
        borderRadius: 3,
        border: '1px solid #333',
        background: `linear-gradient(90deg, ${grad})`,
        flexShrink: 0,
      }}
    />
  );
}

function Row({ label, children }: { label: string; children: React.ReactNode }) {
  return (
    <div style={{ display: 'flex', gap: 8, alignItems: 'center', margin: '6px 0' }}>
      <label style={{ flex: 1, fontSize: 12, color: '#aaa' }}>{label}</label>
      <div style={{ flex: 1.2 }}>{children}</div>
    </div>
  );
}

const input: React.CSSProperties = { width: '100%', background: '#222', color: '#eee', border: '1px solid #333', padding: 4, borderRadius: 4 };
const btn: React.CSSProperties = { ...input, cursor: 'pointer' };
const h3: React.CSSProperties = { margin: '16px 0 6px', fontSize: 12, color: '#888', textTransform: 'uppercase', letterSpacing: 1 };
const hint: React.CSSProperties = { fontSize: 11, color: '#777', margin: '4px 0 8px' };
