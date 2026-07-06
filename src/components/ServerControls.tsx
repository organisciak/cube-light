import { useEffect, useState } from 'react';
import type { AutoCycleSettings, ClientToServer, PatternMeta } from '@shared/types';

interface Props {
  brightness: number;
  gcIntervalSec: number;
  presetNames: string[];
  autoCycle: AutoCycleSettings;
  protocol: 'dnrgb' | 'ddp';
  pacing: { sendMsAvg: number; sendMsMax: number; jitterMsMax: number };
  patterns: PatternMeta[];
  wled: {
    host: string;
    resolvedAddr: string | null;
    sendOk: boolean;
    lastError: string | null;
    lastSendAgoMs: number | null;
    infoOk: boolean;
    infoAgoMs: number | null;
    wledVersion: string | null;
    ledCount: number | null;
  };
  send: (m: ClientToServer) => void;
}

const DEFAULT_CYCLE_LIST = [
  'wavy-sheet', 'plasma', 'rain', 'rotating-planes', 'audio-ripple', 'fire', 'life-3d',
];

/**
 * Panel of project-wide controls: brightness, presets, GC interval, WLED handoff.
 * Sits below the per-pattern controls and is visible on both tabs.
 */
export function ServerControls({ brightness, gcIntervalSec, presetNames, autoCycle, protocol, pacing, patterns, wled, send }: Props) {
  const [presetName, setPresetName] = useState('');
  const [pickedPreset, setPickedPreset] = useState('');
  const [hostDraft, setHostDraft] = useState(wled.host);
  // When the server-confirmed host changes, mirror it into the draft so the
  // input shows the live value. We track the last seen server value so a user
  // mid-edit doesn't get their typing clobbered by unrelated state broadcasts.
  const [lastServerHost, setLastServerHost] = useState(wled.host);
  useEffect(() => {
    if (wled.host !== lastServerHost) {
      setHostDraft(wled.host);
      setLastServerHost(wled.host);
    }
  }, [wled.host, lastServerHost]);

  const sendOk = wled.sendOk && wled.lastSendAgoMs != null && wled.lastSendAgoMs < 2000;
  const infoOk = wled.infoOk && wled.infoAgoMs != null && wled.infoAgoMs < 6000;
  const overallColor = sendOk && infoOk ? '#3da66e' : sendOk || infoOk ? '#d8a13c' : '#c1574a';
  const overallLabel = sendOk && infoOk
    ? 'Connected'
    : sendOk
      ? 'Streaming, but HTTP API not responding'
      : infoOk
        ? 'Reachable, but no UDP frames acknowledged'
        : 'Not connected';

  const cycleable = patterns.filter((p) => p.id !== 'lit-pixel' && p.id !== 'index-walk' && p.id !== 'solid' && p.id !== 'snake-3d');
  const effectiveList = autoCycle.patternList.length > 0 ? autoCycle.patternList : DEFAULT_CYCLE_LIST;
  const isInList = (id: string) => effectiveList.includes(id);
  const togglePattern = (id: string) => {
    const current = autoCycle.patternList.length > 0 ? [...autoCycle.patternList] : [...DEFAULT_CYCLE_LIST];
    const idx = current.indexOf(id);
    if (idx >= 0) current.splice(idx, 1);
    else current.push(id);
    send({ type: 'setAutoCycle', settings: { patternList: current } });
  };

  const applyHost = () => {
    const v = hostDraft.trim();
    if (!v || v === wled.host) return;
    send({ type: 'setWledHost', host: v });
  };

  return (
    <div style={panel}>
      <h3 style={h3}>WLED connection</h3>
      <div style={{ display: 'flex', alignItems: 'center', gap: 8, margin: '4px 0 6px' }}>
        <span
          style={{
            width: 10,
            height: 10,
            borderRadius: '50%',
            background: overallColor,
            boxShadow: `0 0 6px ${overallColor}`,
            flex: '0 0 auto',
          }}
        />
        <span style={{ fontSize: 12, color: '#ddd' }}>{overallLabel}</span>
      </div>
      <div style={{ display: 'flex', gap: 4 }}>
        <input
          type="text"
          value={hostDraft}
          onChange={(e) => setHostDraft(e.target.value)}
          onKeyDown={(e) => { if (e.key === 'Enter') applyHost(); }}
          placeholder="cube.local or 192.168.0.180"
          style={{ ...input, flex: 1, fontFamily: 'ui-monospace, monospace' }}
        />
        <button
          style={btn}
          disabled={!hostDraft.trim() || hostDraft.trim() === wled.host}
          onClick={applyHost}
        >
          Apply
        </button>
      </div>
      <p style={{ fontSize: 10, color: '#888', margin: '4px 0 0', fontFamily: 'ui-monospace, monospace' }}>
        UDP: {sendOk ? 'ok' : 'no'}
        {wled.lastSendAgoMs != null ? ` (${(wled.lastSendAgoMs / 1000).toFixed(1)}s ago)` : ''}
        {' · '}HTTP: {infoOk ? 'ok' : 'no'}
        {wled.resolvedAddr ? ` · ${wled.resolvedAddr}` : ''}
      </p>
      {(wled.wledVersion || wled.ledCount != null) && (
        <p style={{ fontSize: 10, color: '#888', margin: '2px 0 0', fontFamily: 'ui-monospace, monospace' }}>
          {wled.wledVersion ? `WLED ${wled.wledVersion}` : ''}
          {wled.wledVersion && wled.ledCount != null ? ' · ' : ''}
          {wled.ledCount != null ? `${wled.ledCount} LEDs` : ''}
          {wled.ledCount != null && wled.ledCount !== 1000 && (
            <span style={{ color: '#d8a13c' }}> (expected 1000)</span>
          )}
        </p>
      )}
      {wled.lastError && (
        <p style={{ fontSize: 10, color: '#c1574a', margin: '4px 0 0', fontFamily: 'ui-monospace, monospace' }}>
          {wled.lastError}
        </p>
      )}

      <h3 style={h3}>Output</h3>
      <Row label={`Brightness: ${(brightness * 100).toFixed(0)}%`}>
        <input
          type="range"
          min={0}
          max={1}
          step={0.01}
          value={brightness}
          onChange={(e) => send({ type: 'setBrightness', value: Number(e.target.value) })}
          style={input}
        />
      </Row>
      <Row label={gcIntervalSec === 0 ? 'GC sweep: off' : `GC sweep: ${gcIntervalSec}s`}>
        <input
          type="range"
          min={0}
          max={300}
          step={5}
          value={gcIntervalSec}
          onChange={(e) => send({ type: 'setGcInterval', seconds: Number(e.target.value) })}
          style={input}
        />
      </Row>
      <button
        style={{ ...btn, marginTop: 4 }}
        onClick={() => send({ type: 'releaseWled', restoreOn: true })}
        title="Pause our control loop and tell WLED to resume its own effects"
      >
        Release control to WLED
      </button>

      <h3 style={h3}>Auto-cycle on beat</h3>
      <Row label="Enabled">
        <input
          type="checkbox"
          checked={autoCycle.enabled}
          onChange={(e) => send({ type: 'setAutoCycle', settings: { enabled: e.target.checked } })}
        />
      </Row>
      <Row label={`Advance every ${autoCycle.everyNBeats} beats`}>
        <input
          type="range"
          min={1}
          max={32}
          step={1}
          value={autoCycle.everyNBeats}
          onChange={(e) => send({ type: 'setAutoCycle', settings: { everyNBeats: Number(e.target.value) } })}
          style={input}
        />
      </Row>
      <button
        style={{ ...btn, marginTop: 4, width: '100%' }}
        onClick={() => send({ type: 'cycleAdvance' })}
        title="Skip to the next pattern in the cycle list now"
      >
        Cycle now →
      </button>
      <div style={{ marginTop: 4 }}>
        <div style={{ fontSize: 11, color: '#888', marginBottom: 4 }}>Cycle through:</div>
        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 4 }}>
          {cycleable.map((p) => {
            const active = isInList(p.id);
            return (
              <button
                key={p.id}
                onClick={() => togglePattern(p.id)}
                style={{
                  ...chip,
                  background: active ? '#264f8e' : '#1a1a20',
                  borderColor: active ? '#3a6ec5' : '#2a2a2f',
                  color: active ? '#eee' : '#888',
                }}
              >
                {p.name}
              </button>
            );
          })}
        </div>
        {autoCycle.patternList.length === 0 && (
          <p style={{ fontSize: 10, color: '#666', margin: '4px 0 0' }}>(default list)</p>
        )}
      </div>

      <h3 style={h3}>Protocol & pacing</h3>
      <Row label="Protocol">
        <select
          value={protocol}
          onChange={(e) => send({ type: 'setProtocol', protocol: e.target.value as 'dnrgb' | 'ddp' })}
          style={input}
        >
          <option value="dnrgb">DNRGB (WLED native)</option>
          <option value="ddp">DDP (port 4048)</option>
        </select>
      </Row>
      <p style={{ fontSize: 10, color: '#888', margin: '0 0 6px', fontFamily: 'ui-monospace, monospace' }}>
        send: {pacing.sendMsAvg}ms avg / {pacing.sendMsMax}ms max
        {' · '}jitter: {pacing.jitterMsMax}ms max
      </p>

      <h3 style={h3}>Presets</h3>
      <div style={{ display: 'flex', gap: 4 }}>
        <select
          value={pickedPreset}
          onChange={(e) => setPickedPreset(e.target.value)}
          style={{ ...input, flex: 1 }}
        >
          <option value="">— pick —</option>
          {presetNames.map((n) => (
            <option key={n} value={n}>
              {n}
            </option>
          ))}
        </select>
        <button
          style={btn}
          disabled={!pickedPreset}
          onClick={() => pickedPreset && send({ type: 'loadPreset', name: pickedPreset })}
        >
          Load
        </button>
        <button
          style={{ ...btn, color: '#f88' }}
          disabled={!pickedPreset}
          onClick={() => {
            if (pickedPreset && confirm(`Delete preset "${pickedPreset}"?`)) {
              send({ type: 'deletePreset', name: pickedPreset });
              setPickedPreset('');
            }
          }}
        >
          Delete
        </button>
      </div>
      <div style={{ display: 'flex', gap: 4, marginTop: 6 }}>
        <input
          type="text"
          placeholder="Save current as…"
          value={presetName}
          onChange={(e) => setPresetName(e.target.value)}
          style={{ ...input, flex: 1 }}
        />
        <button
          style={btn}
          disabled={!presetName.trim()}
          onClick={() => {
            const n = presetName.trim();
            if (!n) return;
            send({ type: 'savePreset', name: n });
            setPresetName('');
            setPickedPreset(n);
          }}
        >
          Save
        </button>
      </div>
    </div>
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

const panel: React.CSSProperties = {
  marginTop: 12,
  padding: 12,
  background: '#0e0e12',
  border: '1px solid #2a2a2f',
  borderRadius: 6,
};
const h3: React.CSSProperties = {
  margin: '8px 0 6px',
  fontSize: 12,
  color: '#888',
  textTransform: 'uppercase',
  letterSpacing: 1,
};
const input: React.CSSProperties = {
  width: '100%',
  background: '#222',
  color: '#eee',
  border: '1px solid #333',
  padding: 4,
  borderRadius: 4,
};
const btn: React.CSSProperties = { ...input, cursor: 'pointer', width: 'auto', padding: '4px 10px' };
const chip: React.CSSProperties = {
  border: '1px solid',
  borderRadius: 999,
  padding: '3px 10px',
  fontSize: 11,
  cursor: 'pointer',
  background: '#1a1a20',
};
