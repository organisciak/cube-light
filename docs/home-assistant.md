# Home Assistant integration

The firmware speaks MQTT with Home Assistant discovery: point the cube at
your broker and it announces itself — no YAML needed for the entities.

## Setup

1. Have a broker. On Home Assistant OS that's the **Mosquitto broker**
   add-on plus the **MQTT** integration (discovery is on by default).
2. On the cube, open `http://cube.local/wifi` → **Home Assistant (MQTT)**:
   tick *Enable MQTT*, enter the broker host (e.g. `homeassistant.local` or
   its IP), port `1883`, and the MQTT username/password if your broker wants
   one. Save & reboot.
3. A **Cube Light** device appears in HA with four entities:

| Entity | What it does |
| --- | --- |
| `light.cube` | On/off + brightness. "Off" blanks the LEDs (and drops the energy-saving relay on the 618). |
| `select.cube_light_pattern` | Switch the live pattern (full registry). |
| `select.cube_light_preset` | Load a saved preset. Re-announced whenever presets change; hidden until at least one preset exists. |
| `text.cube_light_text` | Sets the text-3d message. Sticks across pattern swaps until reboot; a preset with its own text wins. |
| `switch.cube_light_playlist` | Play/pause the preset cycle — synced with the console, so a manually started cycle shows here too. |
| `switch.cube_light_shuffle` | Shuffle mode (priority-weighted). |
| `button.cube_light_next_preset` / `..._previous_preset` | Skip within the cycle (starts it if stopped). |
| `select.cube_light_playlist_collection` | Which curated playlist the cycle plays; "All presets" = the default pool. |

(Entity ids may differ slightly if you rename things; check the device page.)

## Curated playlists & demo dwell

Playlists are named subsets of the preset store (console → Presets sidebar →
playlist picker; tick presets in or out). `""`/"All presets" keeps today's
behavior. The REST surface: `GET /api/playlists`, `POST /api/playlists/save?name=X`
(body = JSON array of preset names in play order), `POST /api/playlists/delete`,
and `POST /api/playlist?list=X&dwell=N` — `dwell` overrides every preset's
dwell time (5–8s makes a great filming/demo reel; `0` returns to per-preset
times). In curated lists nothing is excluded: priority 0 plays at weight 1.

State flows both ways: changes made on the cube's own console (pattern picks,
playlist advances, brightness) publish back to HA within a moment, plus a 30s
keepalive republish. An availability topic (`cube/avail`, with LWT) grays the
entities out if the cube drops off the network.

## Show the current song title

The cube's 10×10 font is uppercase-only, so upper-case the title. The
trailing spaces give words breathing room between loops.

```yaml
automation:
  - alias: Cube shows current song
    trigger:
      - platform: state
        entity_id: media_player.living_room   # your player
        attribute: media_title
    condition:
      - "{{ trigger.to_state.attributes.media_title is not none }}"
    action:
      - service: select.select_option
        target: { entity_id: select.cube_light_pattern }
        data: { option: text-3d }
      - service: text.set_value
        target: { entity_id: text.cube_light_text }
        data:
          value: "{{ trigger.to_state.attributes.media_title | upper }}  "
```

Tip: turn on the text pattern's **Audio throb** params (`throbDepth` ≈ 0.4)
so the title pulses with the music; `throbRelease` tames the twitchiness.

## Album art (image-3d pattern)

`image-3d` shows a 10×10 downscale of an uploaded image on all four vertical
faces, over a fading background glow in the image's average color — both
throb to the beat.

Upload is HTTP, not MQTT: `POST /api/image?w=<w>&h=<h>` with a base64 body of
raw RGB24 bytes (row-major, top row first; up to 64×64 — the cube
box-averages down to 10×10). Extra query params: `show=1` switches the cube
to image-3d, `persist=0` skips the flash write (use it for once-per-song
pushes; without it the last art also survives reboots).

Home Assistant's container ships `ffmpeg`, which handles the fetch + scale +
raw conversion in one go. `media_player` entity pictures are signed URLs, so
no auth header is needed:

```yaml
shell_command:
  cube_album_art: >-
    /bin/bash -c 'ffmpeg -y -i "http://localhost:8123{{ art }}"
    -vf scale=32:32 -f rawvideo -pix_fmt rgb24 - 2>/dev/null
    | base64 | curl -s -H "Content-Type: application/octet-stream"
    --data-binary @- "http://cube.local/api/image?w=32&h=32&persist=0&show=1"'

automation:
  - alias: Cube shows album art
    trigger:
      - platform: state
        entity_id: media_player.living_room
        attribute: entity_picture
    condition:
      - "{{ trigger.to_state.attributes.entity_picture is not none }}"
    action:
      - service: shell_command.cube_album_art
        data:
          art: "{{ trigger.to_state.attributes.entity_picture }}"
```

(`base64` line-wraps its output; that's fine — the endpoint strips
whitespace. The `Content-Type: application/octet-stream` header matters:
without it the body is parsed as form data and the base64 gets mangled.)

## No broker? REST works too

Everything the MQTT entities do is also plain HTTP on the cube:

```yaml
rest_command:
  cube_text:
    url: "http://cube.local/api/text"
    method: POST
    content_type: application/x-www-form-urlencoded
    payload: "v={{ value }}&show=1"
  cube_power:
    url: "http://cube.local/api/power"
    method: POST
    content_type: application/x-www-form-urlencoded
    payload: "on={{ on }}"
```

Other useful endpoints: `POST /api/pattern` (`id=`), `POST /api/presets/load`
(`name=`), `POST /api/brightness` (`v=0..1`).
