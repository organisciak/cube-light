// Shared clip/comb geometry for the light-cube struts.
// Consumers `include <comb-lib.scad>` and call the comb modules with a GAP LIST
// (n_clips - 1 entries), so uniform 50 mm grids and the alternating end-wrap
// spacing use the same code.
//
// Fixed dimensions here are the ones locked by the v2/v3 fit tests
// (pocket 1.2 on this bead-light wire); heights and mouths stay per-piece args.

$fn = 40;

// ---------- locked by fit tests ----------
// v6 (2026-08-21): pocket tightened 1.2 -> 1.1 after two weeks of real use at
// 2.4 mm / PETG — held fine but could hold better (v2 sweep already rated 1.1
// "tighter, better grip"). Override from the CLI: -D slot=1.2 for the old fit.
slot     = 1.1;    // pocket (v5 was 1.2)
bar_w    = 1.2;    // bar width in the printed plane

// ---------- clip internals ----------
depth    = 5.5;    // clear pocket length (ribbon ~4.8 wide edgewise)
wall     = 1.4;    // clip wall / finger thickness
back     = 1.4;    // back wall
throat   = 1.4;    // narrow mouth section length
flare    = 1.0;    // u-funnel extra half-width
flare_h  = 1.2;    // u-funnel height
c_ramp   = 0.6;    // c-barb entry ramp
c_flare  = 1.2;    // c finger-tip lead-in length
c_flareup= 0.6;    // ...lift at the tip
c_L      = 0.2 + c_ramp + throat + depth + back;   // c station length along x

// clip position i from a gap list (cumulative sum)
function clip_pos(gaps, i) = i <= 0 ? 0 : clip_pos(gaps, i-1) + gaps[i-1];
function span(gaps) = clip_pos(gaps, len(gaps));

// ---------- 2D stations ----------
// U: slot opens +y, back edge on y=0. Funnel -> throat -> square shelf -> pocket.
module u2d(mouth_) {
    PL = slot/2;  ML = mouth_/2;  TL = ML + flare;  O = slot/2 + wall;
    y1 = back;  y2 = back + depth;  y3 = y2 + throat;  y4 = y3 + flare_h;
    polygon([
        [-O,0],[O,0],[O,y4],[TL,y4],[ML,y3],[ML,y2],[PL,y2],[PL,y1],
        [-PL,y1],[-PL,y2],[-ML,y2],[-ML,y3],[-TL,y4],[-O,y4]
    ]);
}

// C: opening faces -x; the bar (y 0..bw) is the lower jaw.
module c2d(mouth_, bw = bar_w) {
    ys = bw + slot;         // finger underside
    yt = ys + wall;         // finger top
    ym = bw + mouth_;       // barb bottom = mouth gap over the bar
    polygon([[-c_flare, yt], [-c_flare, ys + c_flareup], [0, ys],
             [c_L, ys], [c_L, yt]]);                       // finger, lifted tip
    polygon([[0.2, ys + 0.2], [0.2 + c_ramp, ym],          // barb: ramp in,
             [0.2 + c_ramp + throat, ym],                  //  square face back
             [0.2 + c_ramp + throat, ys + 0.2]]);
    translate([c_L - back, 0]) square([back, yt]);         // back wall
}

// ---------- combs (bar integral, no tags) ----------
// bw = bar width in the printed plane. Narrow bars (<= back wall) sit inside the
// clips' back walls as before; wider bars extend BACKWARD (-y) from the pocket
// floor line so they never bury the slot openings.
// ang rotates each clip station relative to the bar (0 = perpendicular slots).
// For diagonal struts the wires stay grid-aligned, so ang = 45: clips sit at
// 45/135 to the bar. Rotated stations are trimmed flush with the bar's bottom
// edge (the pocket stays clear of the trim; the bar backs the wall there).
// A flat part flipped upside-down mirrors ang -> one export serves both
// diagonal directions.
// No `alt` here (unlike c_comb): a U pocket sits ~4 mm to the SIDE of the bar,
// so flipping every second clip to the other side (u_n_u_n_u) would put the
// wires on an 8 mm zigzag instead of one straight grid line. Only the C, whose
// wire lies on the bar's own line, can alternate.
module u_comb(h_, mouth_, gaps, bw = bar_w, ang = 0) {
    O  = slot/2 + wall;
    y0 = bw <= back ? 0 : back - bw;
    linear_extrude(h_) {
        for (i = [0 : len(gaps)]) translate([clip_pos(gaps, i), 0]) {
            intersection() {
                rotate(ang) u2d(mouth_);
                translate([-50, y0]) square([100, 100]);   // trim below bar bottom
            }
            // gusset: an angled station leaves an acute re-entrant notch where
            // its underside meets the bar top — a stress riser on a ~bar_w-wide
            // weld. Fill it with a triangular web: apex anchored inside the
            // clip's outer wall, base overlapping the bar.
            if (ang != 0) {
                a    = abs(ang);
                apex = [ (O - 0.7)*cos(a) - 2.5*sin(a),
                         (O - 0.7)*sin(a) + 2.5*cos(a) ];
                gx   = bw / tan(a) + 3;                     // base reach along bar
                mirror([ang < 0 ? 1 : 0, 0])
                    polygon([apex, [0, bw - 0.2], [gx, bw - 0.2]]);
            }
        }
        translate([-O, y0]) square([span(gaps) + 2*O, bw]);
    }
}

// alt = true mirrors every second station ALONG the bar (about its pocket
// centre, so the 50 mm pitch is unchanged): openings face -x, +x, -x, ...
// The bar is still the lower jaw of every clip, and every wire still sits on
// the bar's line (y = bw + slot/2) -- which is why C can alternate and U can't. A plain C-comb installs by
// sliding ~8 mm along its own axis and comes off the same way; an alternating
// one can't slide off at all, but each wire is pressed over its barb
// individually (odd wires one way, even wires the other).
c_pc = ((0.2 + c_ramp + throat) + (c_L - back)) / 2;       // pocket centre in x
module c_comb(h_, mouth_, gaps, bw = bar_w, alt = false) {
    n  = len(gaps) + 1;
    // station extents: normal [-c_flare, c_L]; mirrored [2*c_pc - c_L, 2*c_pc + c_flare]
    x_lo = -c_flare;
    x_hi = span(gaps) + ((alt && (n-1) % 2 == 1) ? 2*c_pc + c_flare : c_L);
    linear_extrude(h_) {
        for (i = [0 : len(gaps)]) translate([clip_pos(gaps, i), 0]) {
            flip = alt && (i % 2 == 1);
            translate([flip ? 2*c_pc : 0, 0]) mirror([flip ? 1 : 0, 0]) c2d(mouth_, bw);
        }
        translate([x_lo, 0]) square([x_hi - x_lo, bw]);
    }
}
