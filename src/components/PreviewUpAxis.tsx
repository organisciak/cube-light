export type UpAxis = 'x' | 'y' | 'z';

interface Props {
  value: UpAxis;
  onChange: (axis: UpAxis) => void;
}

/**
 * Floating chip selector for "which axis points up in the 3D preview".
 * This is a display-only setting — the buffer sent to WLED is unchanged.
 */
export function PreviewUpAxis({ value, onChange }: Props) {
  return (
    <div style={panel}>
      <span style={label}>Up</span>
      <div style={{ display: 'flex', gap: 4 }}>
        {(['x', 'y', 'z'] as UpAxis[]).map((a) => (
          <button
            key={a}
            onClick={() => onChange(a)}
            style={{ ...chip, ...(value === a ? chipActive : null) }}
            title={`Render ${a.toUpperCase()} as the vertical axis`}
          >
            {a.toUpperCase()}
          </button>
        ))}
      </div>
    </div>
  );
}

const panel: React.CSSProperties = {
  position: 'absolute',
  right: 12,
  top: 56,
  display: 'flex',
  alignItems: 'center',
  gap: 8,
  padding: '6px 8px',
  background: 'rgba(15,15,20,0.85)',
  border: '1px solid #2a2a2f',
  borderRadius: 8,
  backdropFilter: 'blur(4px)',
};
const label: React.CSSProperties = { fontSize: 10, color: '#888', textTransform: 'uppercase', letterSpacing: 1 };
const chip: React.CSSProperties = {
  background: '#1a1a20',
  color: '#888',
  border: '1px solid #2a2a2f',
  borderRadius: 4,
  padding: '3px 8px',
  fontSize: 11,
  cursor: 'pointer',
  fontFamily: 'ui-monospace, monospace',
};
const chipActive: React.CSSProperties = {
  background: '#264f8e',
  color: '#eee',
  borderColor: '#3a6ec5',
};
