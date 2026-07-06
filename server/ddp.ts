import dgram from 'node:dgram';
import { promises as dns } from 'node:dns';

/**
 * DDP (Distributed Display Protocol) sender for WLED.
 *
 * Header (10 bytes):
 *   0: flags  [V1 V0 0 0 T S R Q] — V0 set = protocol version 1; PUSH bit (Q) = commit frame
 *   1: sequence (1..15, 0 = no sequence)
 *   2: data type (1 = RGB)
 *   3: source/output ID (1 = primary)
 *   4-7: data offset in bytes (uint32 BE)
 *   8-9: data length (uint16 BE)
 *
 * For 1000 LEDs at 3 bytes/LED = 3000 bytes the buffer fits in 3 packets at
 * 1440 bytes each. PUSH flag is set on the last packet to commit the frame.
 */
const DDP_PORT = 4048;
const HEADER_SIZE = 10;
const MAX_DATA_PER_PACKET = 1440;
const FLAG_V0 = 0x40;
const FLAG_PUSH = 0x01;

export class DdpSender {
  private socket = dgram.createSocket('udp4');
  private addr: string | null = null;
  private host: string;
  private seq = 0;

  constructor(host: string) {
    this.host = host;
  }

  setHost(host: string) {
    this.host = host;
    this.addr = null;
  }

  async resolve(): Promise<string> {
    if (this.addr) return this.addr;
    if (/^\d+\.\d+\.\d+\.\d+$/.test(this.host)) {
      this.addr = this.host;
    } else {
      const r = await dns.lookup(this.host, { family: 4 });
      this.addr = r.address;
    }
    return this.addr;
  }

  async send(buffer: Uint8Array): Promise<void> {
    const addr = await this.resolve();
    this.seq = (this.seq % 15) + 1;
    // Pre-build all packets, then dispatch in parallel. Each packet carries
    // its own offset so order at the wire doesn't matter; the PUSH flag on
    // the last one is what triggers WLED to commit, and the device handles
    // out-of-order arrivals by buffering.
    const packets: Buffer[] = [];
    let offset = 0;
    while (offset < buffer.length) {
      const remaining = buffer.length - offset;
      const dataLen = Math.min(MAX_DATA_PER_PACKET, remaining);
      const isLast = offset + dataLen >= buffer.length;
      const flags = FLAG_V0 | (isLast ? FLAG_PUSH : 0);
      const pkt = Buffer.alloc(HEADER_SIZE + dataLen);
      pkt[0] = flags;
      pkt[1] = this.seq;
      pkt[2] = 1; // RGB
      pkt[3] = 1; // primary output
      pkt.writeUInt32BE(offset, 4);
      pkt.writeUInt16BE(dataLen, 8);
      pkt.set(buffer.subarray(offset, offset + dataLen), HEADER_SIZE);
      packets.push(pkt);
      offset += dataLen;
    }
    await Promise.all(
      packets.map(
        (pkt) =>
          new Promise<void>((res, rej) =>
            this.socket.send(pkt, DDP_PORT, addr, (err) => (err ? rej(err) : res())),
          ),
      ),
    );
  }

  close() {
    this.socket.close();
  }
}
