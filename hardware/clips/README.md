# Frame clips (3D-printed)

The original cube used the weight of the hanging strings and a bottom clear acrylic plate to keep the planes aligned - it didn't work well.

The improvement was to 3D print translucent combs of spaced clips to add a matrix scaffolding to the cube.
Each comb is a thin bar with a snap clip every 50 mm; the clips grab the string's wire between beads, and the beads themselves
(bigger than any pocket) act as position stops. Stack combs across rows and
columns and the strings hold each other square. Translucent PETG nearly
vanishes when the cube is lit, and what you do see reads as a matrix scaffold.

The combs had either 5 or 6 clips on them - my 3d printer (nor most?) doesn't have a bed big enough to print a single comb that spans the full cube widths, so instead I had LEDs 1-5 clipped, then 5-10.

| File | Clips | Length | Shape |
| --- | --- | --- | --- |
| `strut-u24.stl` | 5 | 204 mm | U: pockets open sideways off the bar |
| `strut-c24.stl` | 5 | 210 mm | C: the bar is the lower jaw, a barbed finger closes over the wire |
| `strut6-u24.stl` | 6 | 254 mm | U, six clips |
| `strut6-c24.stl` | 6 | 260 mm | C, six clips |

All are 2.4 mm thick with a 1.1 mm pocket (sized for the squishy stranded
seed-pixel wire, 4.8 × 1.3 mm) and a 1.2 mm bar.

| U clip | C clip |
| --- | --- |
| ![U clip station](renders/zoom-u.png) | ![C clip station](renders/zoom-c.png) |

## U or C?

Both grab the same wire; they differ in how they load and hold.

- **U** pockets sit beside the bar. Push each wire down into its funnel and it
  clicks past the throat. Easy to load one wire at a time.
- **C** clips lie along the bar, so the whole comb loads in one move: slide it
  about 8 mm along its own axis and all the wires click under the barbs at
  once. The catch is that a comb whose clips all open one way can also slide
  *off* that way. The OpenSCAD source has an `alt=true` option that mirrors
  every second clip so the wires lock it on, at the cost of loading one wire
  per click. On the cube, plain C combs held fine for two weeks of use.

Mix freely: the 5- and 6-clip lengths cover a 10-wide grid in two pieces.

## Printing

- Print **flat as modelled**, no supports. The snap flex has to be in the
  layer plane; printed upright, the fingers would split along layer lines.
- **0.2 mm layers** (the 2.4 mm height is exactly 12 layers). 0.4 mm nozzle.
- **PETG** for the real cube (repeated snap flexing; PLA gets brittle).
  255 °C nozzle, 70 °C bed, 3 walls, brim on.
- **The 6-clip combs only fit a 256 mm bed diagonally.** Rotate 45° in the
  slicer. A bar offset *d* mm from the plate diagonal has 362 − 2*d* mm of
  room, so roughly 15 C combs or 9 U combs fit one plate.
- Handle the bars gently coming off the bed; they are long, thin blades.

## Regenerating

`comb-lib.scad` holds the clip geometry, `strut-set.scad` the comb layouts.
Everything is a parameter: pocket width (`slot`), thickness, clip count,
pitch. For a different wire, print `strut2-*` coupons first (two clips, minutes
each) and step `slot` by 0.1 mm at a time.

```bash
openscad --export-format binstl -o strut-c24.stl -D 'part="c24"' strut-set.scad
openscad --export-format binstl -o strut6-u24.stl -D 'part="u24"' -D n_clips=6 strut-set.scad
openscad --export-format binstl -o strut6-c24-alt.stl -D 'part="c24"' -D n_clips=6 -D alt=true strut-set.scad
```

Other thicknesses from the same source (`part` = `u06` … `u32`, `c06` … `c32`)
were a sweep for preference; thinner combs are nearly invisible but flex more.
2.4 mm is the tested production choice.
