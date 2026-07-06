import dgram from 'node:dgram';
import { NUM_LEDS } from '../src/shared/types.js';

/**
 * Virtual WLED: listens for DNRGB packets on the WLED realtime port and keeps
 * the most recent assembled frame. Lets the C++ firmware core (running as the
 * native harness in firmware/native) drive the browser's 3D preview through
 * this server, with no cube hardware attached.
 *
 * Enable with VIRTUAL_WLED=1. While live packets are arriving (within their
 * timeout), the server's own pattern loop is bypassed and the received frames
 * are broadcast to WS preview clients instead — the same override semantics
 * real WLED applies to our packets.
 */
export class VirtualWled {
  readonly buffer = new Uint8Array(NUM_LEDS * 3);
  private until = 0;

  start(port = 21324) {
    const sock = dgram.createSocket('udp4');
    sock.on('message', (data) => {
      if (data.length < 4 || data[0] !== 0x04) return; // DNRGB only
      const timeoutS = data[1];
      const start = (data[2] << 8) | data[3];
      if (start < 0 || start >= NUM_LEDS) return;
      const count = Math.min(Math.floor((data.length - 4) / 3), NUM_LEDS - start);
      data.copy(this.buffer, start * 3, 4, 4 + count * 3);
      this.until = performance.now() + timeoutS * 1000;
    });
    sock.on('error', (e) => console.error('[virtual-wled] socket error', e.message));
    sock.bind(port, () => {
      console.log(`[virtual-wled] listening for DNRGB on udp/${port}`);
    });
  }

  active(): boolean {
    return performance.now() < this.until;
  }
}
