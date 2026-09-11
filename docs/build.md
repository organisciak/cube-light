# Building the cube

The physical build, as far as this repo knows it. Sections marked **TODO** need
the builder's photos and part links; the electrical facts below come from the
firmware and the boards' recorded configs.

## Bill of materials

| Part | Qty | Notes |
| --- | --- | --- |
| WS2811 12 V seed pixels, 50/string | 20 strings (1000 px) | Tiny "seed"/"fairy" pixels on thin wire, not 12 mm bullets. Search *WS2811 12V seed pixel string*. **TODO: link the exact listing used.** |
| Gledopto GL-C-618WL | 1 | Controller with mic, two outputs, relay, USB-C. [Product page](https://gledopto.com/h-pd-64.html). |
| 12 V PSU, 10 A+ | 1 | Firmware limiter defaults to 10 A. At the playa this ran from a 12 V battery bank. **TODO: which.** |
| Frame | 1 | **TODO:** material, dimensions, how the strings are tensioned. |
| Wire, connectors | | 3-pin JST-SM pigtails are the norm on pixel strings. Inject power at the start of each 500-pixel chain at least. |

## Wiring

- **Two chains of 500.** Output 1 drives pixels 0..499, output 2 drives
  500..999 (`CUBE_LED_SPLIT=500`). Running both halves in the same direction
  from their own start keeps the data path short.
- **Any physical order works.** Strings can run up/down alternately
  (serpentine), start from any corner, and the axes can be in any order. The
  calibration wizard at `/calibrate` figures out the mapping: it lights one
  pixel at a time, you tell it where that pixel is, and after a handful of
  samples the solver has narrowed 192 candidate layouts to one.
- **Color order** is set live from `/leds` (GRB is the default). Same for
  which output is which.
- **Dead pixels.** Cut them out and record how many were snipped from the
  start of each chain on `/leds`; the firmware shifts the data so the rest of
  the geometry stays put.

## Power

- WS2811 seed pixels draw roughly 15 mA at full white (`CUBE_PER_LED_MA`), so
  1000 of them would want ~15 A. The firmware scales brightness to keep the
  frame under `CUBE_SUPPLY_MA` (10 A default, adjustable on `/admin`); the
  sparse patterns this cube favors rarely approach it.
- The 618 has a relay on GPIO18 that cuts LED power when the cube is "off"
  from the console or Home Assistant. It must be driven high for the LEDs to
  light, which the firmware does at boot.
- The controller takes the same 12 V and regulates its own 5 V.

## Controller notes

- **GL-C-618WL**: PDM mic on GPIO32 (data) / GPIO15 (clock), button on 17,
  relay on 18, LED outputs on 16 and 12. Stock WLED config backup in
  `docs/wled-backup/`.
- **GL-C-309WL**: no USB, no mic, single documented output on 16 (a second on
  2), spare pads on 12 and 33. Stock config in `docs/wled-backup-309/`. A PDM
  mic can be added on the spare pads; see the note in `platformio.ini`.
- **GL-C-310WL**: the 309 with an I2S mic (SD 26 / WS 5 / SCK 21).
- **Bare ESP32 + INMP441**: see `[env:esp32dev-inmp441]`. You'll want a
  74HCT-type level shifter on the data lines and a 12 V→5 V buck for the
  board.

## Photos

**TODO.** Frame, string routing, controller mount, the whole thing lit.
