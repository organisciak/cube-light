# cube-light

Patterns, games, and audio-reactive effects for a **10×10×10 WS2811 LED cube**
(1000 LEDs), with a browser-based 3D preview for development and a C++
firmware port that runs the whole thing standalone on the cube's own ESP32.

![3D Pac-Man playing itself in the browser preview](docs/screenshots/pacman-3d.png)

The cube treats its pixels as voxels in a 3D volume: patterns are written
against `(x, y, z)` coordinates and a calibrated mapping translates them to
positions along the physical LED wire. Everything is previewable in the
browser with zero hardware attached.

## What's in here

- **18 patterns** in `src/shared/patterns/` — from ambient (wavy sheet,
  plasma, rotating planes, volumetric cloud, fire, rain, day/night cycle) to
  audio-reactive (spectrum discs, beat ripples) to playable games (3D Snake
  and 3D Pac-Man, both with BFS auto-solvers that play themselves).
- **React UI** (Vite + three.js) with a live 3D preview, per-pattern
  parameter controls, palette selection, presets, auto-cycling, and a
  design-orientation control.
- **Node server** that owns the 30fps pattern loop and streams frames to the
  cube over WLED's DNRGB realtime protocol (UDP), mirroring every frame to
  the browser preview over WebSocket.
- **Browser-mic audio pipeline** — FFT bands + onset (beat) detection feed
  every pattern's `audio` input.
- **A calibration wizard** that solves the physical wiring: it lights one LED
  at a time, you record where it appears, and a solver narrows 192 candidate
  layouts to the unique match.
- **`firmware/`** — the pattern engine ported to dependency-free C++ that
  compiles for both the ESP32 (PlatformIO) and your dev machine, so the cube
  can eventually run everything with no computer attached. See
  [docs/on-chip-plan.md](docs/on-chip-plan.md).

## Screenshots

| | |
|---|---|
| ![Fireworks: white rocket bursting into palette-colored particles](docs/screenshots/fireworks-1.png) | ![Fireworks: a cyberpunk-palette burst dissipating](docs/screenshots/fireworks-2.png) |
| Fireworks — rockets arc in from an edge and burst into palette-colored particles with per-explosion randomness. | Each burst picks its own colors, speeds, and particle lifetimes. |
| ![Calibration wizard lighting a single LED](docs/screenshots/cube-light-with-mic.png) | ![Calibration tab with recorded samples](docs/screenshots/cube-light-calibration.png) |
| Calibration mode lights one raw LED at a time; the mic control feeds the audio-reactive patterns. | The wizard's solver narrows the wiring layout from observed samples. |

## Quick start

Hardware side: a WLED controller (tested on Gledopto ESP32 running WLED
0.15.x) with *Receive UDP realtime* enabled, reachable on your LAN.

```bash
pnpm install
pnpm dev          # web UI on :5273, server on :3037
```

Open http://localhost:5273, pick a pattern, and the cube follows in real
time. No cube on the network? The 3D preview works regardless.

### Useful scripts

- `pnpm dev` — web + server together.
- `pnpm dev:virtual` — same, but the server also *receives* DNRGB on
  udp/21324 and mirrors it to the preview ("virtual WLED"). This is how the
  C++ firmware core is tested without hardware:

```bash
firmware/native/build.sh
firmware/native/build/cube-native pacman-3d --fake-audio
```

…and watch the C++ engine play Pac-Man in the browser preview.

## Architecture

```
┌─────────────┐   WS (frames + control)   ┌──────────────┐   UDP DNRGB    ┌──────────┐
│  React UI    │ ◄───────────────────────► │  Node server  │ ─────────────► │   WLED    │
│  3D preview  │                            │  pattern loop │  3×489-LED     │  ESP32    │
│  (port 5273) │                            │  (port 3037)  │  packets/frame │  1000 LEDs│
└─────────────┘                            └──────────────┘                └──────────┘
                    src/shared/ — patterns, geometry, palettes (used by both)
```

The endgame (in progress, `firmware/`): the pattern loop moves onto the
cube's own ESP32, the mic moves on-chip, and the web stack becomes a dev
tool + remote control. The firmware listens for the same DNRGB packets and
yields to them, so the dev workflow survives unchanged.

## Repo layout

- `src/shared/` — pattern engine, geometry/wiring model, palettes, calibration solver (TypeScript, used by client and server)
- `src/` — React UI
- `server/` — Express + ws + UDP sender, pattern loop, presets, beat detection
- `firmware/lib/core/` — the same engine in pure C++ (all 18 patterns ported)
- `firmware/native/` — host harness: run the C++ engine on your machine, preview in the browser
- `firmware/src/` + `platformio.ini` — ESP32 entry point (GL-C-618WL target)
- `docs/on-chip-plan.md` — the standalone-cube plan
- `data/` — calibration samples and presets (hand-editable)
