# Browser simulator

The firmware's pattern engine, running in your browser, drawn like a 1984
cathode-tube logo card.

`firmware/lib/core` is dependency-free C++, so it compiles unchanged to
WebAssembly. The page loads that module, asks it for 1000 RGB values per frame,
and renders them with three.js through a CRT post-processing chain. Nothing in
`pat_*.cpp` is reimplemented in JavaScript: what you see is what the ESP32
computes, param specs and palettes included (they come out of the same headers
the console uses).

## Running it

It's static files. For local work use the bundled dev server (it disables
caching, which otherwise makes edits to the module scripts invisible):

```bash
python3 simulator/serve.py        # http://127.0.0.1:5277/
```

Hosting: `vercel.json` at the repo root points Vercel at `simulator/web` with
no build step (import the repo in Vercel, or `vercel --prod` from the root).
`.github/workflows/pages.yml` does the same for GitHub Pages once Pages is
enabled with "GitHub Actions" as the source.

- **Playlist**: on load the page cycles a playlist of presets, exactly as the
  cube does — each preset is a pattern plus params, dwell time and a
  music-reactive flag; reactive presets are skipped while audio is off, and
  picking a pattern by hand pauses the cycle. Shuffle (on by default) is the
  cube's weighted pick: each preset's chance is its priority, 0 never
  auto-plays, no immediate repeats. `web/presets.json` is the
  built-in list; *Import presets…* takes the cube's own export
  (`/api/presets/export` → `{"presets":[...]}`) and keeps it in localStorage.
  The wordmark shows the current preset's name unless you type your own.
- **Onboarding**: a first-visit modal offers the microphone (the click is the
  user gesture WebAudio needs), a synthetic beat, or silence; the choice is
  remembered.

- **Pattern** and its parameters: the same knobs as the cube's console.
  `#pattern-id` in the URL selects one on load.
- **Audio**: *microphone* runs the firmware's band analysis (8 log-spaced
  bands, per-band floor/peak tracking) in WebAudio and feeds the bass band to
  the firmware's own `BeatDetector` (in the WASM) for beat and BPM. *Synthetic
  groove* is the native harness's fake audio: a 120 BPM pulse so reactive
  patterns move without a mic.
- **Emulator look** (collapsed by default, saved in localStorage, with a
  reset): LED gain/size, neighbour spill (lit LEDs tint the unlit beads next
  to them), room light (an inside-out sphere and floor that take on the
  cube's mean colour, so a fully lit cube washes the tube and one pixel
  barely registers), bloom, phosphor persistence (a feedback buffer so
  bright voxels leave trails), and the CRT pass: luminance-dependent
  scanlines, aperture-grille mask, barrel curvature with rounded corners,
  chromatic aberration, horizontal beam bleed, vignette, grain, flicker, a
  rolling bar. Untick *CRT treatment* for a clean render.
- **Wordmark**: text under the cube in a block italic with sunset stripes, run
  through the same glow so the card reads like a broadcast ident.
- **Capture**: Record 5 s / 10 s saves a WebM from the canvas, with the
  microphone mixed in when it's on; PNG saves a still.
- **Games**: arrow keys (X/Y) and W/S (Z). The wordmark under the cube
  becomes the legend; while you're driving, the orbit stops and the unlit
  beads brighten so the board reads. 12 s without a key hands the snake back
  to its solver.
- The slow orbit follows the beat: 120 BPM is the base rate, faster music
  spins faster, and each beat nudges it.

## Rebuilding the engine

Only needed when `firmware/lib/core` changes. Needs emscripten
(`brew install emscripten`):

```bash
simulator/build.sh     # -> simulator/web/engine.js + engine.wasm (commit them)
```

`wasm/bridge.cpp` is the whole C API: select pattern, set params, feed audio,
render a frame, read the buffer, plus JSON exports of the param specs and
palette names. Add a function there, and `build.sh` exports it automatically
(it greps the `EMSCRIPTEN_KEEPALIVE` names).

## Files

- `web/index.html`, `style.css` — the page and control panel.
- `web/app.js` — WASM loading, three.js scene (instanced billboard voxels with
  a soft-disc shader, the string lattice, the wordmark), controls, capture.
- `web/crt.js` — `PhosphorPass` and the `CRTShader` composite.
- `web/audio.js` — mic analysis mirroring `firmware/src/audio_capture.cpp`,
  and the synthetic groove.
- `web/engine.js`, `web/engine.wasm` — built output, committed.
- `wasm/bridge.cpp`, `build.sh` — the bridge and its build.
- `serve.py` — no-cache dev server. `web/presets.json` — built-in playlist.

three.js is loaded from a CDN via an import map (pinned version), so there is
no bundler and no `node_modules`.
