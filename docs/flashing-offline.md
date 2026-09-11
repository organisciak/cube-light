# Flashing the cube offline

Everything here works with no internet: the toolchain, the ESP32 platform, and
both prebuilt images are already on this laptop. Written 2026-08-29 on playa.

Prebuilt images live in `firmware/release/`:

| file | board | how it goes on |
| --- | --- | --- |
| `cube-618-2026-08-29.bin` | GL-C-618WL (main) | USB, or armed OTA |
| `cube-618-2026-08-29.factory.bin` | GL-C-618WL (main) | USB only — bootloader + table + app, for a full rebuild at 0x0 |
| `cube-309-2026-08-29.bin` | GL-C-309WL (backup, no USB) | OTA only |

Rebuild either from source with `pio run -e gledopto618` / `-e gledopto309`
(from `firmware/`). Output lands in `.pio/build/<env>/firmware.bin`.

## Linux serial-port access (one-time)

This machine is not set up for serial yet — that's why nothing seemed to
happen when the board was plugged in. Linux has no "allow this device?"
popup like macOS; access is a Unix group instead. The board *is* detected:
a CH340 USB-serial converter at `/dev/ttyUSB0`, owned `root:dialout`, mode
`0660`. The user account is not in `dialout`, so opening the port fails with
`Permission denied`.

```bash
sudo usermod -aG dialout $USER   # permanent — takes effect at next login
sudo chown $USER /dev/ttyUSB0    # takes effect NOW, until the board is unplugged
```

Run both: the first is the real fix, the second avoids logging out mid-session.
Re-run the `chown` after every replug until you have logged out and back in.

Confirm the board is there:

```bash
lsusb | grep -i ch340          # QinHeng Electronics CH340 serial converter
ls -l /dev/ttyUSB0
```

## Flashing the 618 over USB

```bash
cd ~/projects/cube-light/firmware
pio run -e gledopto618 -t upload --upload-port /dev/ttyUSB0
pio device monitor -e gledopto618 --port /dev/ttyUSB0   # 115200; ctrl-] to quit
```

A serial upload writes bootloader + partition table + app. It does **not**
full-erase, and `partitions_cube.csv` keeps NVS at the stock 0x9000/0x5000, so
every saved setting survives: Wi-Fi credentials, AP/console passwords,
per-pattern params, up-axis, LED and mic config, calibration.

### Back up the existing flash first (optional, ~2 min)

```bash
cd ~/projects/cube-light/firmware
~/.platformio/penv/bin/esptool --port /dev/ttyUSB0 --baud 460800 \
    read-flash 0 0x400000 release/618-flash-backup-$(date +%F).bin
```

Restore that image with:

```bash
~/.platformio/penv/bin/esptool --port /dev/ttyUSB0 --baud 460800 \
    write-flash 0 release/618-flash-backup-YYYY-MM-DD.bin
```

### If a flash goes wrong

You cannot brick an ESP32 this way. The first-stage bootloader lives in mask
ROM and cannot be overwritten. Hold the function/BOOT button while plugging
the board in to force download mode, then flash anything — including stock
WLED from `docs/wled-backup/`.

## Updating the 309 (backup board) with no USB

The 309 has no USB port. OTA is the only path, and it is deliberately gated.
Order matters:

1. **Set a non-default AP password.** Open `/wifi` and change it from the
   factory `cubelight`. `/api/ota` refuses to arm while the AP password is
   still the factory default, and OTA reuses the AP password as its own.
2. **Arm the window.** On `/admin`, open the wireless-update window. It lasts
   15 minutes, re-arms on request, and closes on any reboot.
3. **Push the image** within that window:

```bash
python3 ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
    -i cube2.local -p 3232 --auth=YOUR_AP_PASSWORD \
    -f ~/projects/cube-light/firmware/release/cube-309-2026-08-29.bin
```

Use the board's IP instead of `cube2.local` if mDNS is not resolving.

**Size ceiling.** The 309 keeps stock WLED's partition table forever — OTA
replaces an app, it cannot redraw the map. Its app slot is 1.5 MB, versus the
618's custom 1.875 MB. `env:gledopto309` builds against
`partitions_wled_4mb.csv` so the size check fails loudly rather than producing
an image that bricks the board on first boot. Today's build uses 90.4% of that
slot (1421441 of 1572864 bytes) — real headroom, but thin. Check that number
on every 309 build.

**Only ever OTA an image already proven on the 618.** Recovery from a bad
image on the 309 means opening the case and soldering to the serial pads.

## Getting into /admin when there is no home Wi-Fi

Away from the home network the cube falls back to its own AP. Anyone on that
AP subnet is treated as a guest, and guests do not get owner settings.

Log in as username **`cube`**. The password is the console password if one is
set, otherwise the cube's own Wi-Fi (AP) password. The browser prompts for it
when you open `/admin`, `/leds`, `/calibrate` or `/wifi`.

The fallback to the AP password exists so an owner on the fallback AP always
has *some* credential to offer. It is a speed bump, not a secret — everyone on
the AP typed that password to join. Set a real console password from `/wifi`
before handing the cube to a crowd.
