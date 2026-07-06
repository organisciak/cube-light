import type { SnakeAxisFlips } from '../lib/snakeKeys';

/**
 * Floating helper that appears over the preview when the snake pattern is
 * active. Shows the key bindings and lets the user flip each axis so the
 * controls match their seating position relative to the cube.
 */
export function SnakeHint({
  auto,
  flips,
  onFlipsChange,
}: {
  auto?: boolean;
  flips: SnakeAxisFlips;
  onFlipsChange: (f: SnakeAxisFlips) => void;
}) {
  const toggle = (axis: 'x' | 'y' | 'z') =>
    onFlipsChange({ ...flips, [axis]: !flips[axis] });

  return (
    <div style={panel}>
      <div style={title}>3D Snake</div>
      <Row keys={flips.x ? ['→', '←'] : ['←', '→']} extras={flips.x ? ['D', 'A'] : ['A', 'D']} axis="x" flipped={flips.x} onToggle={() => toggle('x')} />
      <Row keys={flips.y ? ['↓', '↑'] : ['↑', '↓']} axis="y" flipped={flips.y} onToggle={() => toggle('y')} />
      <Row keys={flips.z ? ['S', 'W'] : ['W', 'S']} axis="z" flipped={flips.z} onToggle={() => toggle('z')} />
      <div style={{ ...hint, color: auto ? '#3da66e' : '#666' }}>
        {auto ? 'AUTO mode — CPU is playing' : '(click on the page if keys don\'t respond)'}
      </div>
    </div>
  );
}

function Row({
  keys,
  extras,
  axis,
  flipped,
  onToggle,
}: {
  keys: string[];
  extras?: string[];
  axis: string;
  flipped: boolean;
  onToggle: () => void;
}) {
  return (
    <div style={row}>
      {keys.map((k, i) => <Key key={`k${i}`}>{k}</Key>)}
      {extras && (
        <>
          <span style={dim}>/</span>
          {extras.map((k, i) => <Key key={`e${i}`}>{k}</Key>)}
        </>
      )}
      <span style={lbl}>{axis}</span>
      <button
        type="button"
        onClick={onToggle}
        title={flipped ? 'Click to un-flip this axis' : 'Click to flip this axis'}
        style={{ ...flipBtn, ...(flipped ? flipBtnOn : null) }}
      >
        ⇄
      </button>
    </div>
  );
}

function Key({ children }: { children: React.ReactNode }) {
  return <kbd style={kbd}>{children}</kbd>;
}

const panel: React.CSSProperties = {
  position: 'absolute',
  right: 12,
  bottom: 12,
  background: 'rgba(15,15,20,0.85)',
  border: '1px solid #2a2a2f',
  borderRadius: 8,
  padding: 10,
  display: 'flex',
  flexDirection: 'column',
  gap: 4,
  backdropFilter: 'blur(4px)',
  fontFamily: 'ui-monospace, monospace',
};
const title: React.CSSProperties = { fontSize: 11, color: '#888', textTransform: 'uppercase', letterSpacing: 1, marginBottom: 4 };
const row: React.CSSProperties = { display: 'flex', alignItems: 'center', gap: 4, fontSize: 11 };
const kbd: React.CSSProperties = {
  display: 'inline-block',
  background: '#1a1a20',
  border: '1px solid #2a2a2f',
  borderRadius: 4,
  padding: '2px 6px',
  minWidth: 18,
  textAlign: 'center',
  fontFamily: 'ui-monospace, monospace',
  fontSize: 11,
  color: '#ddd',
};
const lbl: React.CSSProperties = { color: '#888', marginLeft: 6, fontSize: 11, minWidth: 10 };
const dim: React.CSSProperties = { color: '#555', fontSize: 11, margin: '0 1px' };
const hint: React.CSSProperties = { fontSize: 9, color: '#666', marginTop: 4 };
const flipBtn: React.CSSProperties = {
  marginLeft: 'auto',
  background: 'transparent',
  border: '1px solid #2a2a2f',
  color: '#666',
  borderRadius: 4,
  padding: '1px 6px',
  cursor: 'pointer',
  fontSize: 10,
  fontFamily: 'inherit',
  lineHeight: 1,
};
const flipBtnOn: React.CSSProperties = {
  background: '#3a6ec5',
  borderColor: '#3a6ec5',
  color: '#fff',
};
