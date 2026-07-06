# Porting cube-light to the chip

**Goal:** the pattern brain runs standalone on the Gledopto ESP32 — no laptop, no
separate computer — using the controller's onboard mic for audio reactivity. The
web app survives as the *development* environment (author patterns in TS with the
3D preview, then port), and optionally as a remote control when a phone is around.

Must-have patterns on-chip: **wavySheet** and **snake3d** (with a way for a third
party to control the snake). Everything else is gravy, ported in order of taste.

## Why not a WLED usermod

WLED's effect API is 1D/2D-shaped: effects iterate pixels in strip order, params
are a handful of 0–255 sliders, and stateful patterns (snake, particles) fight the
per-pixel model. Custom firmware costs us WLED's UI/ecosystem but our patterns port
almost mechanically — they're already `(t, dt, audio, params) → write buffer`
against an `(x,y,z)` index function. Custom firmware is the call. We keep two
WLED-compatible behaviors so the dev workflow survives: a DNRGB listener on 21324
(live frames from the dev server override the local pattern) and an HTTP firmware
upload endpoint (so we can always re-flash, including back to stock WLED).

## Hardware decision

### Dev and primary target: GL-C-618WL ("the big boy")

You already own it, and it's the right unit on every axis:

- **USB-C for firmware download** → a bad flash is a 30-second recovery, never a
  brick. This is what makes it the dev board.
- Onboard PDM mic (WLED's audioreactive runs on it stock — so it demonstrably
  works for our purpose).
- 20A pluggable fuse, reverse-polarity protection, energy-saving relay, DC 5–24V
  input → it takes the 12V bead supply directly and powers its own ESP32. No
  power engineering needed.
- Ethernet + 4 outputs we don't need but don't mind.

**Recommendation: dev on the 618WL and deploy on the 618WL.** It's prebuilt,
fused, and recoverable — exactly what you want in a dusty box on the playa. The
310WL becomes the backup/second unit, not the primary.

### Secondary target: GL-C-310WL (mini)

- Same ESP32 (classic, WiFi + BT 4.2), onboard mic — but it's an **I2S digital
  mic** on SD=GPIO26, WS=GPIO5, SCK=GPIO21. LED data on GPIO16 (also 2, 12).
- **No USB.** Stock flashing is OTA through WLED's update page. If our firmware
  boots broken, there's no serial rescue without cracking the case and soldering
  to the module's pads (recoverable in the lab, not at an event).
- Ship to it **only after** the firmware is stable on the 618WL, and only with the
  self-rescue scheme below in place.

Making OTA-only safe (this is standard ESP32 practice, not exotic):

1. **Two OTA app partitions + rollback** (`CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE`):
   a new image must mark itself valid after boot (e.g. after WiFi comes up and the
   HTTP server answers); if it doesn't, the bootloader reverts to the previous
   image on next reset. A power-cycle undoes a bad flash automatically.
2. **Watchdog** so a hung render loop causes that reset rather than a wedge.
3. First install goes *through WLED's own OTA updater* (it accepts any .bin), which
   means our image must fit the existing WLED partition slot (~1.5MB on the stock
   4MB table) and link against the same partition layout. Fine for our firmware
   size; just don't change the partition table on the USB-less unit.
4. Keep the stock WLED .bin for this exact board on disk. Our firmware exposes
   `/update` (same idea as WLED's) so we can always walk back.

**Pre-flash ritual (both units):** before wiping WLED, open its settings UI and
record every pin mapping it shows (LED GPIO, mic pins, relay pin, button pin).
The stock config is the board's de-facto schematic. Screenshot it into `docs/`.

### The breadboard S3 option: skip it

Not needed — you own two real targets, and the S3 is actually a *worse* match:
the Gledoptos are classic ESP32, so S3-specific work (different I2S peripheral, no
Bluetooth Classic) wouldn't transfer. It would also drag in the power problem for
no benefit. For completeness, the answer to that problem: a **buck converter**
("DC-DC step-down module", e.g. Mini560 or LM2596, ~$2) drops 12V to 5V for a bare
dev board — that's the term you were reaching for — plus a 74AHCT125 to shift the
3.3V data line up to the 5V the WS2811 wants. The Gledopto has both built in,
which is precisely why the prebuilt is right for the playa.

## Firmware architecture

PlatformIO + Arduino-ESP32 (drop to ESP-IDF only if driver timing forces it).

```
firmware/
  src/
    main.cpp            // loop: tick patterns @ 30fps, output, service network
    patterns/           // ported patterns, same shape as TS
    geometry.{h,cpp}    // port of src/shared/geometry.ts (12 lines of math)
    orientation.{h,cpp} // 24-rotation post-transform
    palettes.{h,cpp}
    audio.{h,cpp}       // I2S/PDM mic → FFT → 8 bands + beat envelope
    net/
      api.{h,cpp}       // HTTP + WS, mirrors ServerToClient/ClientToServer JSON
      dnrgb.{h,cpp}     // UDP 21324 listener: live frames override local brain
      ota.{h,cpp}       // /update endpoint + rollback confirm
    store.{h,cpp}       // LittleFS: layout, presets, last pattern+params
```

Pattern interface mirrors the TS one so porting is a translation, not a redesign:

```cpp
struct PatternCtx {
  uint8_t* buffer;                 // NUM_LEDS*3 RGB
  uint16_t (*idx)(uint8_t,uint8_t,uint8_t);
  float t, dt;
  const AudioFrame& audio;         // level, bands[8], beat
  const Params& params;
};
```

- **LED output:** NeoPixelBus or FastLED via RMT. 1000 WS2811 at 30fps is
  comfortable; the known risk is WiFi vs. LED-timing contention — stress test in
  week one, tune task priorities if needed.
- **Audio (in scope from the start, per the goal):** 618WL = PDM mic, 310WL = I2S
  mic on GPIO26/5/21. Read via the ESP32 I2S peripheral, FFT with ESP-DSP or
  arduinoFFT, collapse to the same 8 bands + beat-envelope math currently in
  `server/index.ts`. WLED's audioreactive usermod is open source and runs on
  these exact boards — crib its mic-init code rather than rediscovering I2S
  quirks.
- **Geometry/orientation/palettes:** direct ports, all small. The 192-candidate
  calibration solver stays host-side; the firmware just accepts a resolved
  `Layout` via the API and persists it.
- **API:** ESPAsyncWebServer serving the same WS JSON contract as
  `server/index.ts`, so the existing React app can point at `cube.local` for
  control and preview. The firmware also serves a small static controller page
  (below) from LittleFS.

## Snake control (third-party device)

Two tiers:

1. **Phone as controller (do this one):** the firmware serves a self-contained
   D-pad page at `http://cube.local/snake` — six buttons sending `snakeInput`
   over the same WS. Zero extra hardware, works for any guest with a phone on the
   cube's network. At the playa the ESP32 runs as its own WiFi AP (`cube-light`,
   no internet needed); anyone joins and plays. ~150 lines of HTML total.
2. **Bluetooth gamepad (stretch):** classic ESP32 does Bluetooth, and Bluepad32
   pairs PS4/Xbox/8BitDo pads directly. Caveat: WiFi+BT coexistence costs RAM and
   can jitter LED timing — prototype only after tier 1 works, and be ready to
   drop it. A BLE-only pad (8BitDo in BLE mode) coexists better than BT Classic.

## Development workflow after the port

The web stack stays, repurposed:

- **Authoring:** write/tune a pattern in TS against the Vite preview exactly as
  today, with the Node server sending DNRGB to the cube. The firmware yields to
  live packets automatically, so "dev mode" needs no mode switch on the cube.
- **Porting:** translate the finished TS pattern to C++ (mechanical for the pure-
  math ones). Flash over USB (618WL) or OTA.
- **Remote control:** the React app pointed at `cube.local` manages the on-chip
  brain — pattern select, params, presets, brightness — from any phone/laptop.

Yes, this means each shipped pattern exists twice (TS + C++). With no interpreter
on-chip that's the honest cost; the mirrored `PatternCtx` keeps the translation
diff-like. (A Pixelblaze-style on-chip script engine was considered and cut —
you said you don't need it, and it's the multi-month part.)

## Fallback considered: keep the brain external on a tiny computer

A Raspberry Pi Zero 2 W (~$20) runs the existing Node server unmodified, powered
from the same 12V through a USB buck adapter, and a phone can join its hotspot
for the React UI. Zero porting. It's a legitimate plan B — but it's a second
device, an SD card that can corrupt on hard power-loss, ~30s boot, and two things
to power and debug in the dust. The on-chip port is more work up front and much
more robust at the event. Going on-chip.

## Porting order

1. **Skeleton week (on the 618WL, USB attached):** WiFi + OTA + LittleFS + LED
   output + `wavySheet` ported + DNRGB listener. Success = cube runs wavy-sheet
   standalone at 30fps with no flicker, and the dev server can still take over.
2. **Audio:** mic → bands/beat, wire into wavySheet's audio distortion. Success =
   unplug everything, clap, cube reacts.
3. **API + persistence:** WS contract, params, presets, brightness, last-state
   restore on boot. React app controls the cube directly.
4. **snake3d + controller page.** BFS auto-solver ports as-is (1000-cell graph is
   nothing); manual mode via the D-pad page.
5. **Rest of the catalog by taste:** plasma, rotatingPlanes, fireworks (cap the
   particle pool), pacman3d, fire, rain, life3d, text3d…
6. **310WL bring-up (optional):** rollback partitions proven on the 618WL first,
   then OTA the mini, config-switch the pins (LED GPIO16, mic 26/5/21).

Step 1 is the go/no-go gate: if 1000 LEDs + WiFi hold 30fps on the Gledopto, the
rest is scheduled work, not research.

## Sources

- [GL-C-618WL product page](https://gledopto.com/h-pd-64.html) — PDM mic, USB-C
  program download, relay, fuse, DC 5–24V.
- [GL-C-618WL manual (PDF)](https://www.ledbe.com/image/catalog/gledopto/GL-C-618WL/Gledopto-ESP32-Elite-Advanced-WLED-Controller-User-Manual.pdf)
- [GL-C-309WL/310WL spec (PDF)](https://www.superlightingled.com/PDF/SPEC/GL-C-309WL+GL-C-310WL.pdf) —
  I2S mic pins SD=26 WS=5 SCK=21, LED GPIO16/2/12, 10A max.
- [WLED forum: Inside GLEDOPTO GL-C-310WL](https://wled.discourse.group/t/inside-gledopto-gl-c-310wl/15188) — teardown photos.
- [Gledopto WLED web flasher](https://gledopto.com/h-col-381.html) — stock
  firmware recovery images.
