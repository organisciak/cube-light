# cube-light

**A 10×10×10 LED cube that runs itself.** One thousand addressable pixels, an
ESP32 that lives inside the cube, a phone-friendly console, and a microphone so
the whole thing dances to whatever is playing. No laptop, no app store, no
cloud: plug it into 12 V and it boots into a playlist.

![3D Pac-Man playing itself](docs/screenshots/pacman-3d.png)

It went to the playa in 2026 and ran for a week off a battery bank, with
strangers picking patterns from their phones over the cube's own Wi-Fi hotspot.
This repo is everything needed to build one: the firmware, the console, the
calibration tools, and (soon) the physical build notes.

## What it does

- **28 patterns** written against `(x, y, z)` voxels rather than a strip: wavy
  sheet, rotating planes, rain, comets, storm cloud, spirals, orbiting atoms,
  a tumbling wireframe cube, a first-person neon rail ride, a synesthetic
  tunnel, a double helix, TV static, fireworks, a 3D text marquee, album art
  on all four faces, and more.
- **Music-reactive.** An onboard mic feeds an FFT, eight log-spaced bands, a
  loudness envelope and a beat detector into every pattern. Patterns choose
  what to do with it: throb, spin faster, spawn ripples, kick the rail.
- **Games.** 3D Snake and 3D Pac-Man with a phone game pad (or they play
  themselves with a BFS solver when nobody is holding the controls).
- **Presets and playlists.** Save any pattern + params + palette as a preset,
  weight it, and let the cube cycle. Curated playlists, shuffle, a demo dwell
  for filming, and a physical button that steps to the next preset.
- **Guest mode.** Anyone who joins the cube's hotspot gets patterns, knobs,
  brightness and the game pad. Settings, Wi-Fi and calibration stay behind an
  owner password. Flip one switch and the hotspot password is the only gate.
- **Home Assistant.** MQTT discovery publishes a light, pattern and preset
  selects, playlist transport, and a text entity. Song titles and album art
  from your media player show up on the cube. See
  [docs/home-assistant.md](docs/home-assistant.md).
- **Per-parameter modulation.** Any numeric knob can ride an LFO or a random
  walk, so a static preset keeps evolving.
- **Calibration that solves your wiring.** Light one LED, say where it is,
  repeat a few times; the cube works out axis order, flips and serpentine
  runs from 192 candidate layouts. Snipped-LED compensation keeps geometry
  correct after you cut a dead pixel out of a chain.
- **Runs on ~$20 of controller.** A Gledopto WLED controller straight out of
  the box, or any classic ESP32 plus an I2S microphone.

| | |
|---|---|
| ![Fireworks](docs/screenshots/fireworks-1.png) | ![Fireworks, cyberpunk palette](docs/screenshots/fireworks-2.png) |
| ![Calibration lighting one LED](docs/screenshots/cube-light-with-mic.png) | ![Calibration samples](docs/screenshots/cube-light-calibration.png) |

*Screenshots are from the retired browser preview; the same patterns run on
the cube. A browser simulator that renders the real firmware output is on the
roadmap (below).*

## Building one

Three parts: a cube of LEDs, a controller, and this firmware.

### 1. The cube

- **1000 × 12 V WS2811 "seed" pixels**, wired as two chains of 500. Seed
  pixels (the tiny ones on thin wire, not 12 mm bullets) matter: the cube
  reads as points of light in air rather than a wall of bulbs. Search for
  *WS2811 12V seed pixel string*; BTF-Lighting, Ray Wu and the usual
  AliExpress storefronts all sell 50-pixel strings.
- A frame that holds 10 vertical strings of 100 pixels each in a 10×10 grid.
  Any wiring order works because calibration solves it afterwards.
- A 12 V supply. The firmware's power limiter caps total draw (default 10 A);
  full white on 1000 pixels would be ~15 A, so the limiter matters.

Build notes, photos and the parts list are in
[docs/build.md](docs/build.md).

### 2. The controller

| Board | Mic | Flashing | Notes |
| --- | --- | --- | --- |
| **Gledopto GL-C-618WL** (recommended) | onboard PDM | USB-C | Two outputs, function button, power relay. The board the project was developed on. |
| Gledopto GL-C-310WL | onboard I2S | OTA only (no USB) | Mini version. Untested but pins are per the manual. |
| Gledopto GL-C-309WL | **none** | OTA only (no USB) | Same as the 310 without a mic. Runs ambient patterns; the console says so. |
| Any classic ESP32 + INMP441 | I2S | USB | Wire your own; pins are build flags. |

Every board is one `[env:...]` block in
[firmware/platformio.ini](firmware/platformio.ini): LED pins, button, relay,
mic type and pins, mDNS name. Copy one and edit for new hardware.

### 3. The firmware

```bash
cd firmware
pio run -e gledopto618 -t upload      # first flash over USB
pio device monitor -e gledopto618     # 115200 baud
```

The cube comes up as a Wi-Fi hotspot named `cube-light` (password
`cubelight`, change it). Join it, open <http://cube.local/> or
<http://192.168.4.1/>, and pick a pattern. The `/wifi` page joins it to your
home network; after that, wireless updates are available from `/admin`.

No-USB boards (309/310) get their first image through stock WLED's update
page. [docs/flashing-offline.md](docs/flashing-offline.md) is the full
runbook, including recovery and the OTA size ceiling.

## The console

Everything is served from the cube itself, sized for a phone.

- `/` picks patterns (grouped into light patterns, games, and tools), exposes
  each pattern's parameters with hover help, palettes, brightness, the mic
  toggle, the preset sidebar and the playlist transport. Advanced mode reveals
  modulation and the audio-drive caps.
- `/snake` is the game pad. Direction keys are calibrated to the player's
  viewpoint so "up" means up from where you stand.
- `/admin` handles power, wireless update arming, presets import/export and
  the demo dwell. `/leds` sets outputs, color order, snipped-LED compensation
  and the mic channel/squelch. `/calibrate` runs the wiring wizard. `/wifi`
  holds network, passwords, guest mode and the Home Assistant broker.
- `/api/status`, `/api/audio` and friends are plain JSON if you want to
  script it. DNRGB packets on udp/21324 (WLED's realtime protocol) override
  the local pattern, so anything that can drive WLED can drive the cube.

## Running the engine on your computer

The pattern engine is dependency-free C++ shared between the ESP32 build and
a native harness:

```bash
firmware/native/build.sh
firmware/native/build/cube-native rez-tunnel --fake-audio --host 192.168.0.180
```

It streams DNRGB frames to a cube, to stock WLED, or to any receiver that
speaks the protocol. See [firmware/README.md](firmware/README.md) for the
engine layout and how to add a pattern.

## Roadmap

- **Browser simulator.** A three.js page that renders the firmware's frames
  with real light bloom, takes your laptop mic, and exports short scrubbable
  clips, because LED cubes photograph terribly.
- **Build documentation** with photos, the frame, and a bill of materials.
- ESP-NOW hardware controller for Snake ([docs/controller-plan.md](docs/controller-plan.md)).

Open issues live in the repo's `.beads/` tracker.

## Repo layout

- `firmware/lib/core/` — the pattern engine (pure C++): geometry, palettes,
  params, audio analysis, one file per pattern.
- `firmware/src/` — ESP32 glue: networking, web console, mic capture, presets,
  Home Assistant.
- `firmware/native/` — host harness.
- `docs/` — flashing runbook, HA integration, build notes, stock-WLED config
  backups for the boards, screenshots.

The original React + Node prototype that streamed frames to stock WLED is
preserved at git tag `wled-prototype`.

## License

MIT. See [LICENSE](LICENSE).
