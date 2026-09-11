# cube-light firmware

Standalone brain for the cube: the pattern engine running on the Gledopto
ESP32 itself (GL-C-618WL target). See `docs/on-chip-plan.md` for the full plan.

## Layout

- `lib/core/` — the pattern engine, **pure C++** (no Arduino headers):
  geometry, palettes, color, params, audio analysis, and the patterns
  themselves. Compiles identically for the ESP32 and for the host.
- `src/main.cpp` — ESP32 entry point: WiFi, NeoPixelBus (RMT) LED output,
  DNRGB live-override listener on udp/21324, ArduinoOTA. Pins come from
  `platformio.ini` build flags — check them against stock WLED's settings
  for any new board before first flash.
- `native/` — host harness. Compiles `lib/core` with clang++ and streams
  frames as WLED DNRGB packets, so the exact firmware pattern code can be
  run with zero hardware (point it at a real cube, stock WLED, or a
  DNRGB-speaking simulator).
- `platformio.ini` — ESP32 build config (`pio run -e gledopto618 -t upload`).
- `src/ha_mqtt.h` — Home Assistant bridge (MQTT discovery: light + pattern /
  preset selects + text entity). Configure from `/wifi`; see
  `docs/home-assistant.md` for automations (song titles, album art).

## Running the engine without hardware

```bash
firmware/native/build.sh
firmware/native/build/cube-native wavy-sheet --fake-audio --host 192.168.0.180
```

The harness renders the exact firmware pattern code on your machine and sends
DNRGB packets (udp/21324) to `--host` (default `127.0.0.1`). Any DNRGB receiver
works: a cube running this firmware (its live-override listener takes the
frames), stock WLED with *Receive UDP realtime* on, or a simulator. A
browser-based simulator that receives these frames is planned (see the beads
issue tracker); the retired React preview that used to fill this role lives at
git tag `wled-prototype`.

Harness usage:

```
cube-native [patternId] [key=value ...] [--fps N] [--seconds S] [--fake-audio]
            [--host IP] [--port P]
```

`key=value` pairs map to the pattern's params (`true`/`false` → bool, numbers
→ number, else string), e.g. `cube-native wavy-sheet palette=arctic amp=2`.
Unset params use the defaults from `cube_param_specs.h`.

Caveat: the harness renders with the **default layout**; a receiver with a
calibrated layout active will look scrambled.

## Adding a pattern

1. Copy the closest `pat_*.cpp` and write the render body against
   `PatternCtx` (`cube_pattern.h`): `t`, `dt`, `audio`, `params`, and the
   `setIdx(x,y,z)` voxel index. Params are read with inline defaults
   (`p.num("amp", 1.2f)`).
2. Register it in `cube_patterns.cpp` and add its spec block to
   `cube_param_specs.h` (that block drives the web UI's controls).
3. `firmware/native/build.sh && cube-native <id> --fake-audio` against a cube
   or simulator, then an on-cube pass — LED cubes read very differently from
   a screen (sparse, high-contrast designs win).

### Pattern param specs

`lib/core/cube_param_specs.h` (parameter names/types/ranges/defaults for the
web UI) is **hand-maintained and authoritative** — edit it directly when
adding or changing a pattern's params.

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
