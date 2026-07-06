interface Props {
  value: string;
  onChange: (color: string) => void;
}

const PRESETS = ['#0a0a0c', '#1f1f24', '#444444', '#bbbbbb', '#ffffff'];

/**
 * Floating top-right swatch picker for the 3D preview background.
 * Persisted in localStorage by the parent.
 */
export function PreviewBackground({ value, onChange }: Props) {
  return (
    <div style={panel}>
      <span style={label}>BG</span>
      <div style={{ display: 'flex', gap: 4 }}>
        {PRESETS.map((c) => (
          <button
            key={c}
            onClick={() => onChange(c)}
            title={c}
            style={{
              ...swatch,
              background: c,
              outline: value.toLowerCase() === c.toLowerCase() ? '2px solid #3a6ec5' : 'none',
            }}
          />
        ))}
        <input
          type="color"
          value={value}
          onChange={(e) => onChange(e.target.value)}
          style={picker}
          title="Custom color"
        />
      </div>
    </div>
  );
}

const panel: React.CSSProperties = {
  position: 'absolute',
  right: 12,
  top: 12,
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
const swatch: React.CSSProperties = {
  width: 18,
  height: 18,
  borderRadius: 4,
  border: '1px solid #2a2a2f',
  cursor: 'pointer',
  padding: 0,
};
const picker: React.CSSProperties = {
  width: 22,
  height: 22,
  border: '1px solid #2a2a2f',
  borderRadius: 4,
  background: 'transparent',
  cursor: 'pointer',
  padding: 0,
};
