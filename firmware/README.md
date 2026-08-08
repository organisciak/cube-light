# cube-light firmware

Standalone brain for the cube: the pattern engine running on the Gledopto
ESP32 itself (GL-C-618WL target). See `docs/on-chip-plan.md` for the full plan.

## Layout

- `lib/core/` — the pattern engine, **pure C++** (no Arduino headers). Ports of
  `src/shared/`: geometry, palettes, color, params, and the patterns
  themselves. Compiles identically for the ESP32 and for the host.
- `src/main.cpp` — ESP32 entry point: WiFi, NeoPixelBus (RMT) LED output,
  DNRGB live-override listener on udp/21324, ArduinoOTA. **Unverified until a
  board is on the desk** — check `CUBE_LED_PIN` and the color order against
  stock WLED's settings before first flash.
- `native/` — host harness. Compiles `lib/core` with clang++ and streams
  frames as WLED DNRGB packets, so the exact firmware pattern code can be
  previewed with zero hardware.
- `platformio.ini` — ESP32 build config (`pio run -e gledopto618 -t upload`).
- `src/ha_mqtt.h` — Home Assistant bridge (MQTT discovery: light + pattern /
  preset selects + text entity). Configure from `/wifi`; see
  `docs/home-assistant.md` for automations (song titles, album art).

## Emulating without hardware

Terminal 1 — web app + server with the virtual cube enabled:

```bash
pnpm dev:virtual        # = VIRTUAL_WLED=1 pnpm dev
```

Terminal 2 — build and run the firmware core natively:

```bash
firmware/native/build.sh
firmware/native/build/cube-native wavy-sheet --fake-audio
```

The harness sends DNRGB to `127.0.0.1:21324`; the server (in virtual-WLED
mode) receives it and mirrors frames to the browser's 3D preview at
`http://localhost:5273`, overriding its own pattern loop — the same
live-override semantics real WLED applies. Stop the harness and the server's
own patterns resume after the 2s timeout.

Harness usage:

```
cube-native [patternId] [key=value ...] [--fps N] [--seconds S] [--fake-audio]
            [--host IP] [--port P]
```

`key=value` pairs map to the pattern's params (`true`/`false` → bool, numbers
→ number, else string), e.g. `cube-native wavy-sheet palette=arctic amp=2`.
Unset params use the same defaults as the TS versions.

Caveat: the harness renders with the **default layout**; if the previewing
server has a calibrated layout active, the preview will look scrambled. Keep
the server on the default layout when emulating (or teach the harness the
calibrated layout when that matters).

## Porting a pattern from TS

1. Copy the closest `pat_*.cpp`, translate the TS render body — the
   `PatternCtx` fields match `src/shared/patterns/types.ts` one-for-one.
   Params are read with inline defaults (`p.num("amp", 1.2f)`) mirroring the
   TS `num(params.amp, 1.2)` idiom.
2. Register it in `cube_patterns.cpp`.
3. `firmware/native/build.sh && cube-native <id>` against `pnpm dev:virtual`
   and eyeball it next to the TS original.

Ported so far: `wavy-sheet`, `plasma`, `rotating-planes`, `solid`.

### Pattern param specs

`lib/core/cube_param_specs.h` (parameter names/types/ranges/defaults for the
web UI) is now **hand-maintained and authoritative** — edit it directly when
adding or changing a pattern's params. It was originally generated from the TS
patterns by `scripts/gen-param-specs.mts`, but the TS side is deprecated and
that generator is **retired**: do not re-run it, as it would clobber
firmware-only params (e.g. the spiral `cycle` axis).

Two shared blocks get spliced into pattern spec arrays instead of being
retyped, so the same knob means the same thing everywhere:

- `CUBE_THROB_SPECS(defaultDepth)` — the beat-driven brightness throb
  (`cube_throb.h`): `throbDepth` / `throbAttack` / `throbRelease`.
- `CUBE_PALETTE_SOLO_SPECS` — single-color palette mode (`cube_palettes.h`):
  `paletteSolo` / `soloSpeed`. On every pattern that has a `palette` param.

### Single-color ("solo") palettes

With `paletteSolo` on, a palette stops being a spatial gradient and becomes a
journey: the whole pattern takes **one** palette color per frame and drifts
through the palette at `soloSpeed` passes/second. Picking `rainbow` walks the
cube through the colors of the rainbow one at a time; `fire` walks red →
orange → yellow → white and back.

It lives in the palette layer — `resolvePalette(name, params, now)` bakes the
drifting position into the `PaletteRef` and `samplePalette()` ignores the
per-voxel `t` — so every palette-bearing pattern gets it for free. Sampled
colors are renormalized to full value in this mode, since most gradients ramp
out of black and a solo sweep through that end would just dim the cube.

## Snipped-LED compensation

Burnt-out LEDs get physically cut off a chain; without compensation, a snip at
the *start* of an output shifts every surviving LED's data by that many
positions and scrambles the calibrated geometry. The `/leds` page (and
`/api/ledcfg` args `skip1/trim1/skip2/trim2`, persisted in NVS) records how
many LEDs each output lost from its start/end: `show()` then feeds each data
slot the logical LED that physically sits there, so patterns stay
geometry-correct and the snipped spots simply go dark. End trims are recorded
for completeness but need no shift.

To rehearse on an intact cube, the same page has a snip **simulator**
(`/api/ledsim`, runtime-only — reboot clears it): it makes a chain behave as
if N LEDs were cut. Simulate N, watch the pattern slide out of place, set
"snipped from start" to N, and it should snap back with N dark spots.

## Hardware gotchas (learned the hard way)

- **Arduino core 2.x WiFi crash-loop on mesh networks.** The stock PlatformIO
  `espressif32` platform is frozen at arduino-esp32 2.x (IDF 4.4), whose WiFi
  stack dies with `InstructionFetchError` in `sta_recv_mgmt` /
  `offchan_recv_action` when a router sends 802.11k/v roaming action frames —
  i.e., the instant it joins a mesh network as a client. AP mode never
  triggers it (nobody sends the cube those frames), which made it look like
  "joining WiFi broke the cube." Fixed by the pioarduino platform pin in
  platformio.ini (arduino-esp32 3.x / IDF 5.5).
- **Startup ordering matters.** Network services (mDNS/OTA/UDP/HTTP) and the
  mic start from `startNetServices()` only after an interface is up.
- **GPIO18 relay must be HIGH** or the LED string has no power (looks exactly
  like a data-wiring failure).
- Debug flags for bisecting on-hardware problems:
  `-DCUBE_LED_BISECT_DISABLE`, `-DCUBE_MIC_BISECT_NO_I2S`,
  `-DCUBE_MIC_BISECT_NO_TASK`.
