import type { ClientToServer, Orientation } from '@shared/types';

interface Props {
  orientation: Orientation;
  send: (m: ClientToServer) => void;
}

/**
 * Floating panel: three buttons that rotate the design 90° around X/Y/Z, plus a reset.
 * Anchored bottom-left of the 3D preview.
 */
export function OrientationControls({ orientation, send }: Props) {
  const id = isIdentity(orientation);
  return (
    <div style={panel}>
      <div style={label}>Rotate output</div>
      <div style={row}>
        <RotBtn axis="x" color="#f87171" onClick={() => send({ type: 'rotateOrientation', axis: 'x' })} />
        <RotBtn axis="y" color="#4ade80" onClick={() => send({ type: 'rotateOrientation', axis: 'y' })} />
        <RotBtn axis="z" color="#60a5fa" onClick={() => send({ type: 'rotateOrientation', axis: 'z' })} />
      </div>
      <button
        style={{ ...resetBtn, opacity: id ? 0.4 : 1 }}
        onClick={() => send({ type: 'resetOrientation' })}
        disabled={id}
      >
        Reset
      </button>
    </div>
  );
}

function RotBtn({ axis, color, onClick }: { axis: 'x' | 'y' | 'z'; color: string; onClick: () => void }) {
  return (
    <button style={btn} onClick={onClick} title={`Rotate 90° about ${axis.toUpperCase()}`}>
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke={color} strokeWidth="2.4" strokeLinecap="round" strokeLinejoin="round">
        <path d="M21 12a9 9 0 1 1-3.5-7.1" />
        <polyline points="21 4 21 9 16 9" />
      </svg>
      <span style={{ color, fontWeight: 600, fontSize: 11, marginLeft: 4 }}>{axis.toUpperCase()}</span>
    </button>
  );
}

function isIdentity(o: Orientation): boolean {
  return (
    o[0][0] === 1 && o[0][1] === 0 && o[0][2] === 0 &&
    o[1][0] === 0 && o[1][1] === 1 && o[1][2] === 0 &&
    o[2][0] === 0 && o[2][1] === 0 && o[2][2] === 1
  );
}

const panel: React.CSSProperties = {
  position: 'absolute',
  left: 12,
  bottom: 12,
  background: 'rgba(15,15,20,0.85)',
  border: '1px solid #2a2a2f',
  borderRadius: 8,
  padding: 8,
  display: 'flex',
  flexDirection: 'column',
  gap: 6,
  backdropFilter: 'blur(4px)',
};
const label: React.CSSProperties = {
  fontSize: 10,
  color: '#888',
  textTransform: 'uppercase',
  letterSpacing: 1,
};
const row: React.CSSProperties = { display: 'flex', gap: 6 };
const btn: React.CSSProperties = {
  background: '#1a1a20',
  border: '1px solid #2a2a2f',
  borderRadius: 6,
  padding: '6px 10px',
  cursor: 'pointer',
  display: 'inline-flex',
  alignItems: 'center',
};
const resetBtn: React.CSSProperties = {
  background: 'transparent',
  color: '#aac6f3',
  border: '1px solid #2a2a2f',
  borderRadius: 6,
  padding: '4px 8px',
  cursor: 'pointer',
  fontSize: 11,
};
