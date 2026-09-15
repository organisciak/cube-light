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

It's static files. Any web server will do:

```bash
cd simulator/web && python3 -m http.server 5277
# open http://127.0.0.1:5277/
```

Or open the GitHub Pages deployment once it's enabled for the repo
(`.github/workflows/pages.yml` publishes `simulator/web` on every push).

- **Pattern** and its parameters: the same knobs as the cube's console.
  `#pattern-id` in the URL selects one on load.
- **Audio**: *microphone* runs the firmware's band analysis (8 log-spaced
  bands, per-band floor/peak tracking) in WebAudio and feeds the bass band to
  the firmware's own `BeatDetector` (in the WASM) for beat and BPM. *Synthetic
  groove* is the native harness's fake audio: a 120 BPM pulse so reactive
  patterns move without a mic.
- **Look**: LED gain/size, bloom, phosphor persistence (a feedback buffer so
  bright voxels leave trails), and the CRT pass: luminance-dependent
  scanlines, aperture-grille mask, barrel curvature with rounded corners,
  chromatic aberration, horizontal beam bleed, vignette, grain, flicker, a
  rolling bar. Untick *CRT treatment* for a clean render.
- **Wordmark**: text under the cube in a block italic with sunset stripes, run
  through the same glow so the card reads like a broadcast ident.
- **Capture**: Record 5 s / 10 s saves a WebM from the canvas; PNG saves a
  still. Games take arrow keys (X/Y) and W/S (Z).

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

three.js is loaded from a CDN via an import map (pinned version), so there is
no bundler and no `node_modules`.
