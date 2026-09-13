// Light-cube strut set — v4: production-candidate combs, thickness sweep
//
// Findings driving this version:
//   - pocket 1.2 confirmed perfect (v3 U1.2 comb)
//   - happy accident: combs from the spaghetti-failed plates — only a couple of
//     layers tall — turned out to work WELL for this use. So part thickness (Z)
//     becomes the swept variable, and thin is a feature: near-invisible struts.
//   - thinner parts flex more, so the mouth TIGHTENS as thickness drops to keep
//     retention (paired lists below).
//   - no more label tags — pieces are told apart by their thickness.
//
// 5 U-combs + 5 C-combs, 5 clips each at 50 mm pitch.
// Intended use: thicker struts on the cube's outer layers, thinner inside.
// Heights are exact multiples of 0.2 -> slice at 0.2 mm layers.
//
// Per-object export (so any comb can be skipped mid-print):
//   for p in u06 u10 u16 u24 u32 c06 c10 c16 c24 c32:
//     openscad -o strut-$p.stl -D 'part="$p"' strut-set.scad
//   6-clip:           add -D n_clips=6          (254 / 260 mm -> place diagonally)
//   alternating:      add -D alt=true           (C only: openings flip clip to clip)
//   v5 pocket (1.2):  add -D slot=1.2           (v6 default is 1.1)
// v6 exports live in v6/ (see README) so the printed v5 files stay untouched.
//
// Clip geometry lives in comb-lib.scad (shared with end-strut.scad).

include <comb-lib.scad>

part = "all";   // "all" or one of u06|u10|u16|u24|u32|c06|c10|c16|c24|c32
alt  = false;   // true: C-combs alternate opening direction along the bar (see comb-lib).
                // Ignored for U-combs -- their pockets sit beside the bar, so flipping
                // would zigzag the wires off the grid line.
                // pocket width: comb-lib's `slot` (v6 = 1.1); -D slot=1.2 for the v5 fit

bar_width = 1.2;   // standard 1.2; 3.6 is the stiffer option:
                   //   openscad -o strut-w36-<part>.stl -D bar_width=3.6 -D 'part="..."' strut-set.scad

// ---------- the sweep: thickness -> mouth (paired) ----------
// v5 mouths (field-tested 2026-07-08): thin struts are so flexible the mouth can
// pinch far tighter — 0.45 up through 1.6 mm; 0.75 at 3.2+; 2.4 interpolated.
heights = [0.6, 1.0, 1.6, 2.4, 3.2];      // part height (Z); thin = inner layers
mouths  = [0.45, 0.45, 0.45, 0.60, 0.75]; // snap gap, tighter when thinner
ids_u   = ["u06", "u10", "u16", "u24", "u32"];
ids_c   = ["c06", "c10", "c16", "c24", "c32"];

n_clips = 5;
pitch   = 50;
grid_gaps = [for (i = [1 : n_clips-1]) pitch];

module show(p) { if (part == "all" || part == p) children(); }

for (i = [0 : len(heights)-1]) {
    show(ids_u[i]) translate([0, 13*i,     0]) u_comb(heights[i], mouths[i], grid_gaps, bar_width);
    show(ids_c[i]) translate([0, 70 + 9*i, 0]) c_comb(heights[i], mouths[i], grid_gaps, bar_width, alt = alt);
}
