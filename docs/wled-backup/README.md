# Stock WLED backup — GL-C-618WL ("cube2-temp"), 2026-07-12

Snapshot of the factory WLED 0.15.3 config before flashing custom firmware.
`cfg.json` is the authoritative machine-readable backup (restorable via
WLED's settings → Security & Updates → restore). Screenshots are the
human-readable copy.

## Pin map (from cfg.json — the board's de-facto schematic)

| function | GPIO | notes |
|---|---|---|
| LED output 1 | 16 | factory: 4 outputs × 30 LEDs, type WS281x |
| LED output 2 | 12 | |
| LED output 3 | 4  | |
| LED output 4 | 2  | |
| Function button | 17 | push, active-low, internal pullup |
| **Relay (LED power)** | **18** | active high — **must be driven HIGH or the string is dark** |
| Mic (I2S PDM) | DATA=32, CLK=15 | AudioReactive "digitalmic" type 5 |
| IR | — | disabled |
| Ethernet | type 13; pins 21/19/22/25/26/27/5/23/33/0 | unused by our firmware |

Other factory settings of note:

- Color order: 0 = **GRB**
- ABL: maxpwr 850mA (factory default, not our PSU's real budget)
- AudioReactive usermod enabled: squelch 4, gain 60, AGC 2, freq scale 3
- USB-UART chip: WCH CH340-family, VID 0x1A86 PID 0x7522 (needs WCH's
  CH34x VCP driver on macOS; Apple's built-in driver doesn't cover this PID)

## Files

- `cfg.json` — full WLED config (the restore file)
- `presets.json` — presets (factory-empty)
- `info.json` — /json/info snapshot (version, arch)
- `settings-{leds,sync,um,wifi,sec}.png` — settings pages
