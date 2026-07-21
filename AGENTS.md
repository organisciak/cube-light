# cube-light

React + Node prototyping environment for a 10×10×10 WS2811 LED cube driven by WLED.

## Hardware

- WLED 0.15.3 on ESP32 (Gledopto controller), 1000 LEDs, RGB.
- mDNS: `cube.local` (currently `192.168.0.180`).
- UDP realtime port: `21324`.

## Architecture

- `vite` dev server (port **5273**) — React UI with a 3D cube preview.
- `server/` (port **3037**) — Express + ws + UDP. Owns the pattern loop, sends DNRGB packets to WLED, mirrors frames to WS clients for the preview.

(Both ports are non-default to avoid clashes with other local dev servers. Override with `PORT=` for the server, or edit `vite.config.ts` for the web port.)
- Pattern engine and geometry mapping live in `src/shared/` so client and server agree on pixel layout.

## Realtime protocol

WLED DNRGB (0x04) over UDP:

```
[0x04, timeout_secs, start_high, start_low, R0, G0, B0, R1, G1, B1, ...]
```

- Max ~489 LEDs per packet (header 4B + payload ≤ 1467B fits MTU comfortably).
- For 1000 LEDs we send 3 packets per frame. The first packet's `timeout` byte applies to the whole frame; WLED reverts to its own effects after `timeout` seconds with no packets.

## Scripts

- `pnpm dev` — concurrent web + server.
- `pnpm dev:web` / `pnpm dev:server` — individually.
- `pnpm dev:virtual` — like `dev` but the server also listens for DNRGB on
  udp/21324 and mirrors received frames to the preview (virtual WLED). Used to
  preview the C++ firmware core with no cube attached.

## Firmware (on-chip port)

`firmware/` holds the standalone ESP32 port (target: Gledopto GL-C-618WL).
`firmware/lib/core/` is the pattern engine in pure C++ — shared verbatim
between the ESP32 build (`platformio.ini`) and a native host harness
(`firmware/native/`) that streams DNRGB for hardware-free previewing.
See `firmware/README.md` and `docs/on-chip-plan.md`. When porting a TS
pattern, keep the C++ line-for-line faithful to `src/shared/patterns/`.

Pattern param specs (`firmware/lib/core/cube_param_specs.h`) are now
**hand-maintained and authoritative** — edit that header directly. It was once
generated from the TS patterns by `scripts/gen-param-specs.mts`, but the TS side
is deprecated and that generator is **retired**; do not re-run it (it would
clobber firmware-only params such as the spiral `cycle` axis).

## Layout calibration

The mapping from `(x, y, z)` to LED index depends on how the strings are wired.
`src/shared/geometry.ts` exposes `makeIndex(layout)` with axis permutation, per-axis
flip, and serpentine flags. The Calibrate tab in the UI runs a wizard: it lights one
LED at a time, the user records its (x,y,z), and the server's solver narrows the 192
candidate layouts to the unique match. Samples persist to `data/calibration.csv` and
are hand-editable / copy-pastable.

`src/shared/orientation.ts` adds an independent post-pattern transform (24-orientation
rotation group) so the user can rotate the *design* — what gets sent to the cube — via
the X / Y / Z buttons in the bottom-left corner of the preview. Calibration is wiring;
orientation is "which way is up?".

## Pixel hygiene

Each frame already sends all 1000 LEDs (3 DNRGB packets per frame), so no LED can be
"stuck on" longer than one dropped packet. Still, observe these rules so a misstep
doesn't strand pixels:

- **At mode transitions** (entering/exiting calibration, pause, pattern swap on
  user request): call `blackout()` on the server. It zeroes the buffer, broadcasts
  one frame, and pushes one DNRGB packet — guarantees a clean visual transition
  rather than relying on the next loop tick.
- **On pause** (`setRunning=false`): we send a single all-off frame and stop the
  loop. Without this, WLED would keep showing the last frame for `timeoutSecs`
  (currently 2s) before reverting to its own effects.
- **Periodic deep clean (low priority, todo)**: even though every frame is a full
  refresh, an occasional explicit "GC" pass — e.g. one all-off frame inserted
  every N seconds, or one round of two frames `(black, current)` — would catch
  any LED whose state drifted due to repeated packet loss in a tiny corner of
  the cube. Tracked as a beads issue. Keep it low-priority because correctly-
  sized DNRGB packets and DDP both already hit every LED on every send.
- **When growing patterns**: do NOT design any pattern that "remembers" lit
  pixels by skipping writes. The buffer is reset implicitly only inside each
  pattern's `render()`. If a pattern wants persistence (rain trails, fire), it
  must own the decay logic and write every LED every frame.

## Landing the Plane (Session Completion)

**When ending a work session**, you MUST complete ALL steps below. Work is NOT complete until `git push` succeeds.

**MANDATORY WORKFLOW:**

1. **File issues for remaining work** - Create issues for anything that needs follow-up
2. **Run quality gates** (if code changed) - Tests, linters, builds
3. **Update issue status** - Close finished work, update in-progress items
4. **PUSH TO REMOTE** - This is MANDATORY:
   ```bash
   git pull --rebase
   bd sync
   git push
   git status  # MUST show "up to date with origin"
   ```
5. **Clean up** - Clear stashes, prune remote branches
6. **Verify** - All changes committed AND pushed
7. **Hand off** - Provide context for next session

**CRITICAL RULES:**
- Work is NOT complete until `git push` succeeds
- NEVER stop before pushing - that leaves work stranded locally
- NEVER say "ready to push when you are" - YOU must push
- If push fails, resolve and retry until it succeeds
