interface Props {
  active: boolean;
  level: number;
  bands: number[];
  beat: number;
  error: string | null;
  onStart: () => void;
  onStop: () => void;
}

export function MicControl({ active, level, bands, beat, error, onStart, onStop }: Props) {
  return (
    <div style={panel}>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
        <button
          style={{ ...btn, background: active ? '#7e1d1d' : '#264f8e' }}
          onClick={active ? onStop : onStart}
          title={active ? 'Stop microphone' : 'Start microphone'}
        >
          {active ? '■ Stop mic' : '🎤 Mic'}
        </button>
        <Meter value={level} />
        <BeatDot value={beat} />
      </div>
      {active && (
        <div style={bandsRow}>
          {bands.map((v, i) => (
            <div key={i} style={bandWrap}>
              <div style={{ ...bandFill, height: `${Math.round(v * 100)}%` }} />
            </div>
          ))}
        </div>
      )}
      {error && <p style={errStyle}>{error}</p>}
    </div>
  );
}

function Meter({ value }: { value: number }) {
  return (
    <div style={meterOuter}>
      <div style={{ ...meterFill, width: `${Math.round(value * 100)}%` }} />
    </div>
  );
}

function BeatDot({ value }: { value: number }) {
  // Flash dot intensity tracks the beat envelope.
  const intensity = Math.round(value * 100);
  return (
    <div
      title="Beat (server's onset detector)"
      style={{
        width: 14,
        height: 14,
        borderRadius: '50%',
        background: `rgb(${235 - 100 * (1 - value)}, ${50 + 80 * value}, ${50})`,
        boxShadow: value > 0.05 ? `0 0 ${4 + intensity / 4}px rgba(239, 68, 68, ${value})` : 'none',
        transition: 'all 50ms linear',
      }}
    />
  );
}

const panel: React.CSSProperties = {
  marginTop: 12,
  padding: 8,
  background: '#0e0e12',
  border: '1px solid #2a2a2f',
  borderRadius: 6,
};
const btn: React.CSSProperties = {
  background: '#264f8e',
  color: '#eee',
  border: '1px solid #3a6ec5',
  borderRadius: 4,
  padding: '6px 10px',
  cursor: 'pointer',
  fontSize: 12,
  whiteSpace: 'nowrap',
};
const meterOuter: React.CSSProperties = {
  flex: 1,
  height: 6,
  background: '#1a1a20',
  borderRadius: 3,
  overflow: 'hidden',
};
const meterFill: React.CSSProperties = {
  height: '100%',
  background: 'linear-gradient(90deg, #4ade80 0%, #f5a524 75%, #ef4444 100%)',
  transition: 'width 60ms linear',
};
const bandsRow: React.CSSProperties = {
  display: 'flex',
  gap: 4,
  alignItems: 'flex-end',
  height: 60,
  marginTop: 8,
};
const bandWrap: React.CSSProperties = {
  flex: 1,
  height: '100%',
  background: '#1a1a20',
  borderRadius: 2,
  display: 'flex',
  alignItems: 'flex-end',
};
const bandFill: React.CSSProperties = {
  width: '100%',
  background: 'linear-gradient(180deg, #60a5fa, #3a6ec5)',
  borderRadius: 2,
};
const errStyle: React.CSSProperties = { color: '#f88', fontSize: 11, margin: '6px 0 0' };
