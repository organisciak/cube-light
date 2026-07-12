# Snake controller: a dedicated ESP32 remote

A second ESP32 with physical buttons in a 3D-printed case, talking to the
cube over **ESP-NOW**. Companion to `docs/on-chip-plan.md`.

## Why ESP-NOW (and not WiFi/Bluetooth)

ESP-NOW is Espressif's connectionless protocol at the WiFi MAC layer —
ESP-to-ESP directly, no router, no association, no TCP. Properties that
matter here:

- **Latency: 1–4ms** per packet. For comparison: joining the cube's AP and
  sending UDP is ~5–20ms plus reconnect stalls; Bluetooth Classic gamepads
  via Bluepad32 work but WiFi+BT coexistence on the ESP32 costs RAM and can
  jitter the LED timing.
- **Coexists with the cube's AP/STA mode** — the cube keeps serving its web
  page and receiving DNRGB while also receiving ESP-NOW.
- Controller can **deep-sleep** between play sessions and run months on a
  small battery.

## Will it be responsive enough?

Yes, with margin. The full chain:

| stage | time |
|---|---|
| debounce | ~5–10ms |
| ESP-NOW transmission | ~1–4ms |
| cube processes input (every `loop()` pass) | <1ms |
| **added latency total** | **~10–15ms** |
| game applies input at next tick (4–12Hz) | up to 83–250ms |

The game's own tick rate dominates by an order of magnitude — the snake
steps on a grid at 4–12Hz, so input feel is identical to a keyboard on the
dev setup. 10–15ms is also well under a single 30fps frame.

One design rule to make this true: the controller must **stay awake during
play**. Deep-sleep wake costs 100–300ms of boot, which WOULD be felt if it
happened per-press. So: wake on first press (that one press eats the boot
delay — acceptable, it's "picking up the controller"), stay awake while
buttons are active, deep-sleep after ~5 minutes idle.

## The 6-direction problem (and the fun answer)

3D snake needs six directions (x±, y±, z±). Layout options:

1. **Cube-shaped controller, one button per face.** Press the face the
   snake should go. Thematically perfect for a cube of cubes, dead-simple
   mapping, a great 3D-print. Downside: the face in your palm is awkward to
   press — mitigate by making it palm-sized and pressed like a fidget toy
   with both hands.
2. **Flat pad: D-pad cross (4) + two thumb buttons** for z± (like L/R
   shoulders). Most ergonomic, most conventional.
3. **Joystick module + 2 buttons.** Analog stick reads as 4 directions,
   buttons for z±. Fewer holes to print, slightly mushier feel.

Recommendation: build 2 (proven ergonomics) but seriously consider 1 for
the playa — guests understand it instantly with zero explanation.

Viewer-relative vs. cube-relative mapping: keep it cube-relative (buttons
map to cube axes) and rely on the existing direction-hint overlay (the
colored 3×3 flash on the face you pressed toward) to teach guests — it's
already in the firmware port.

## Hardware (≈ $10 total)

- **ESP32-C3 SuperMini** (~$3) — tiny (22×18mm), USB-C for flashing,
  ESP-NOW capable, deep-sleep friendly. Any ESP32 works; the C3 is the
  smallest cheap option.
- **6× 12mm tactile switches** (or Cherry MX–style for nicer feel; MX
  keycaps also print well).
- **LiPo 500mAh + TP4056 USB-C charge board** (~$2), or a 2×AAA holder if
  charging circuitry feels like overkill.
- Slide switch (hard power-off for transport), optional status LED.
- 3D-printed two-shell case; buttons wired to GPIOs with internal pullups,
  common ground. No other components needed — debounce is software.

## Protocol

One packet per press, broadcast (no pairing ceremony):

```
{ magic: 0xC0BE, seq: uint16, button: uint8 }   // button = SnakeDir 0..5
```

- Controller sends each press 3× at 5ms spacing (broadcast has no MAC-level
  ack); the cube dedupes on `seq`. Effectively lossless.
- Cube-side receive callback maps `button` straight to `queueSnakeInput()` /
  `queuePacmanInput()` (both already exist in `firmware/lib/core`), keyed on
  the active pattern.
- Later hardening: after first packet, cube can note the controller's MAC
  and the controller can switch to unicast (which does get MAC-level acks).

**The one real gotcha — WiFi channel.** ESP-NOW peers must be on the same
channel. When the cube runs its own AP (playa mode) the channel is pinned —
trivial. When the cube joins home WiFi, its channel follows the router, so
the controller boots by channel-scanning 1–11, sending a discovery ping on
each until the cube answers, then locks in. ~30 lines and a one-time
~200ms scan per controller boot.

## Software plan

1. `firmware/src/espnow_input.{h,cpp}` (cube side): init ESP-NOW alongside
   the existing WiFi mode, receive callback → validate magic → dedupe seq →
   queue input for the active game. ~60 lines.
2. `firmware/controller/` + a second PlatformIO env (`board = esp32-c3`):
   button scan + debounce, press → 3× broadcast, idle timer → deep sleep
   (EXT1 wake on any button), channel scan on boot. ~150 lines.
3. Case: OpenSCAD model once button layout is picked (option 1 or 2 above).

Testing needs two boards in hand — this is post-flash-day work. The
cube-side receive path can land earlier since it's inert without a sender.

## Alternatives already covered elsewhere

- **Phone D-pad page** (`/snake` on the cube's web server — planned): zero
  hardware, works for any guest, ~20–50ms latency over the AP. Do this
  regardless; the ESP-NOW remote is the premium tactile option on top.
- **BLE gamepad (8BitDo etc.) via Bluepad32**: stretch goal, watch RAM and
  LED-timing jitter from BT coexistence.
- **Wired buttons** into the 618WL's spare IOs (12/4/2/13): simplest
  possible, but a tethered controller is a tripping hazard in the dark.
