import dgram from 'node:dgram';
import { promises as dns } from 'node:dns';

const WLED_PORT = 21324;
/** DNRGB sub-protocol byte. WLED treats this as start-indexed RGB stream. */
const DNRGB = 0x04;
/** Max LEDs per DNRGB packet. WLED docs say 489. */
const MAX_LEDS_PER_PACKET = 489;

export interface WledOptions {
  host: string;
  /** Seconds WLED keeps the realtime override before reverting. 1..255. */
  timeoutSecs?: number;
}

export class WledSender {
  private socket = dgram.createSocket('udp4');
  private addr: string | null = null;
  private host: string;
  private timeoutSecs: number;
  private resolving: Promise<string> | null = null;

  constructor(opts: WledOptions) {
    this.host = opts.host;
    this.timeoutSecs = opts.timeoutSecs ?? 2;
  }

  getHost(): string {
    return this.host;
  }

  setHost(host: string) {
    this.host = host;
    this.addr = null;
    this.resolving = null;
  }

  async resolve(): Promise<string> {
    if (this.addr) return this.addr;
    if (this.resolving) return this.resolving;
    this.resolving = (async () => {
      // Accept a literal IP straight through.
      if (/^\d+\.\d+\.\d+\.\d+$/.test(this.host)) {
        this.addr = this.host;
      } else {
        const r = await dns.lookup(this.host, { family: 4 });
        this.addr = r.address;
      }
      return this.addr!;
    })();
    return this.resolving;
  }

  /**
   * Send a full RGB buffer (length = numLeds*3). Splits into DNRGB chunks
   * and dispatches them in parallel — UDP order isn't guaranteed anyway
   * (each DNRGB packet carries its own start_index) so serial awaits would
   * just add latency without any ordering benefit.
   */
  async send(buffer: Uint8Array): Promise<void> {
    const addr = await this.resolve();
    const numLeds = buffer.length / 3;
    const sends: Promise<void>[] = [];
    for (let start = 0; start < numLeds; start += MAX_LEDS_PER_PACKET) {
      const count = Math.min(MAX_LEDS_PER_PACKET, numLeds - start);
      const pkt = Buffer.alloc(4 + count * 3);
      pkt[0] = DNRGB;
      pkt[1] = this.timeoutSecs;
      pkt[2] = (start >> 8) & 0xff;
      pkt[3] = start & 0xff;
      pkt.set(buffer.subarray(start * 3, (start + count) * 3), 4);
      sends.push(
        new Promise<void>((res, rej) =>
          this.socket.send(pkt, WLED_PORT, addr, (err) => (err ? rej(err) : res())),
        ),
      );
    }
    await Promise.all(sends);
  }

  /**
   * Ensure WLED is in the "on" state so live-mode packets actually drive the LEDs.
   * Without this, if WLED has been put into off state (e.g. by saving LED
   * preferences with "Turn on at boot" off, or by a "stay off" preset), our DDP/
   * DNRGB packets land in live mode but produce no visible output — the device-
   * level on/off gate suppresses them.
   */
  async seize(): Promise<void> {
    const addr = await this.resolve();
    const url = `http://${addr}/json/state`;
    const ctl = new AbortController();
    const timer = setTimeout(() => ctl.abort(), 1500);
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({ on: true }),
        signal: ctl.signal,
      });
      if (!res.ok) throw new Error(`WLED returned ${res.status}`);
    } finally {
      clearTimeout(timer);
    }
  }

  /**
   * Tell WLED to release realtime control via its HTTP JSON API.
   * Without this, the device stays in "live" mode for `timeoutSecs` after
   * we stop sending DNRGB; this nudges it to revert to its own effects now.
   * `restoreOn` controls whether the lights stay on (using whatever effect
   * was active before we took over) or are turned off.
   */
  async release(restoreOn: boolean = true): Promise<void> {
    const addr = await this.resolve();
    const url = `http://${addr}/json/state`;
    const body = JSON.stringify({ live: false, on: restoreOn });
    const ctl = new AbortController();
    const timer = setTimeout(() => ctl.abort(), 1500);
    try {
      const res = await fetch(url, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body,
        signal: ctl.signal,
      });
      if (!res.ok) throw new Error(`WLED returned ${res.status}`);
    } finally {
      clearTimeout(timer);
    }
  }

  close() {
    this.socket.close();
  }
}
