import type { ClientToServer, ServerToClient } from '@shared/types';

export type Listener = (msg: ServerToClient) => void;

const FRAME_OPCODE = 1;

export function connect(onMessage: Listener): { send: (m: ClientToServer) => void; close: () => void } {
  const url = `${location.protocol === 'https:' ? 'wss' : 'ws'}://${location.host}/ws`;
  let ws: WebSocket | null = null;
  let alive = true;
  let backoff = 500;

  const open = () => {
    ws = new WebSocket(url);
    ws.binaryType = 'arraybuffer';
    ws.onopen = () => {
      backoff = 500;
    };
    ws.onmessage = (e) => {
      if (typeof e.data === 'string') {
        try {
          onMessage(JSON.parse(e.data) as ServerToClient);
        } catch {
          /* ignore */
        }
        return;
      }
      // Binary frame: [opcode, seq32 BE, rgb...]
      if (e.data instanceof ArrayBuffer && e.data.byteLength >= 5) {
        const view = new DataView(e.data);
        if (view.getUint8(0) !== FRAME_OPCODE) return;
        const seq = view.getUint32(1, false);
        const rgb = new Uint8Array(e.data, 5);
        onMessage({ type: 'frame', seq, rgb });
      }
    };
    ws.onclose = () => {
      if (!alive) return;
      setTimeout(open, backoff);
      backoff = Math.min(5000, backoff * 1.5);
    };
    ws.onerror = () => ws?.close();
  };
  open();

  return {
    send(m) {
      if (ws && ws.readyState === WebSocket.OPEN) ws.send(JSON.stringify(m));
    },
    close() {
      alive = false;
      ws?.close();
    },
  };
}
