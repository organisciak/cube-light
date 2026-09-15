# cube-light

Firmware for a 10×10×10 WS2811 LED cube (1000 voxels) that runs standalone on
an ESP32: pattern engine, audio reactivity from an onboard mic, a phone-friendly
web console, presets/playlists, games, and a Home Assistant bridge.

The original React/Node prototype (browser 3D preview + Node server streaming
DNRGB to stock WLED) was removed once the firmware became the sole source of
truth. It is preserved at git tag `wled-prototype` if you need to consult it.

## Hardware

- Primary: Gledopto GL-C-618WL (ESP32-WROOM, USB-C, onboard PDM mic, relay).
  mDNS `cube.local`.
- Backup (`cube2.local`): ordered as a Gledopto GL-C-310WL (no USB, OTA-only,
  I2S mic on SD 26 / WS 5 / SCK 21). It still runs the 0.4.3 `gledopto309`
  image, whose PDM driver reads a dead line, so it has no reactivity until
  the `gledopto310` image is flashed (cube-8p2; waiting for 0.5.0 to be
  proven on the 618 first). `gledopto309` remains for mic-less boards.
- Any classic ESP32 + an I2S/PDM mic works; pins are `-D` build flags in
  `firmware/platformio.ini`.
- LEDs: two chains of 500 on separate outputs (`CUBE_LED_SPLIT`).

## Layout

- `firmware/lib/core/` — the pattern engine in **pure C++** (no Arduino
  headers). Geometry, palettes, params, audio analysis, and one `pat_*.cpp`
  per pattern. Compiles identically for the ESP32 and the host.
- `firmware/src/` — ESP32 glue: `main.cpp` (WiFi/AP fallback, LED output,
  web server + auth tiers, playlist, button, OTA), `web_ui.h` (the console
  HTML/JS as C strings), `audio_capture.cpp` (I2S mic → FFT → `AudioFrame`),
  `cube_presets.*` (LittleFS preset store), `ha_mqtt.h` (Home Assistant).
- `firmware/native/` — host harness: builds `lib/core` with clang++ and
  streams frames as WLED DNRGB packets (udp/21324) for hardware-free preview.
- `firmware/platformio.ini` — one `[env:...]` per board with pin flags.
- `simulator/` — browser simulator. `wasm/bridge.cpp` + `build.sh` compile
  `lib/core` to WebAssembly with emscripten (`brew install emscripten`; rebuild
  and commit `web/engine.{js,wasm}` whenever lib/core changes). `web/` is
  static: three.js from a CDN import map, CRT shaders in `crt.js`, mic
  analysis in `audio.js`. Serve with `python3 -m http.server 5277`.
- `docs/` — flashing runbooks, HA integration, stock-WLED config backups,
  screenshots. `docs/on-chip-plan.md` and `docs/controller-plan.md` are
  design notes, the first historical.

## Conventions

- **Param specs are hand-maintained** in `firmware/lib/core/cube_param_specs.h`
  and authoritative for the web UI. Edit that header when adding or changing a
  pattern's params.
- **Every pattern writes every LED every frame.** No pattern may "remember" lit
  pixels by skipping writes; persistence (rain trails, decay) is the pattern's
  own job inside `render()`.
- **Mode transitions blackout.** Entering/exiting calibration, pause, or a
  user-requested pattern swap sends one all-off frame first.
- Audio-reactive terms must be scaled; see the `throb` and `audio` helpers.
- Guest vs owner: anyone on the cube's own AP is a guest (patterns, params,
  brightness, game pad) unless they prove ownership with the console password
  (or the AP password when none is set). Owner-only pages *challenge* with
  Basic auth; owner-only API routes return a friendly 403.

## Building and flashing

```bash
cd firmware
~/.platformio/penv/bin/pio run -e gledopto618            # build
~/.platformio/penv/bin/pio run -e gledopto618 -t upload  # first flash over USB
firmware/native/build.sh && firmware/native/build/cube-native wavy-sheet --fake-audio
```

`pio` may not be on PATH — use the penv path above. OTA after the first flash
is gated: set a non-default AP password, arm the window from `/admin`, then
`espota.py`. See `docs/flashing-offline.md`. The 309 keeps stock WLED's 1.5 MB
app slot: watch its size check on every build.

## Quality gates

- Both envs build: `pio run -e gledopto618 -e gledopto309`.
- Native harness builds: `firmware/native/build.sh`.
- Anything touching LED output, mic, WiFi, or auth gets an on-cube pass
  before it is called done (file a beads issue if a cube isn't at hand).

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
