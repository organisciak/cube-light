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
