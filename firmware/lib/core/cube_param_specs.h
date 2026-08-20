#pragma once
// AUTHORITATIVE / HAND-MAINTAINED. Edit this file directly when adding or
// changing firmware pattern params.
//
// This file was ORIGINALLY generated from the TS patterns in
// src/shared/patterns/* by scripts/gen-param-specs.mts, back when the TS side
// was the single source of truth. That TS side is now deprecated and the
// generator (scripts/gen-param-specs.mts) is RETIRED — do NOT re-run it against
// this file. It would clobber firmware-only additions (e.g. the spiral "cycle"
// axis option) that no longer exist on the TS side.
#include <cstdint>

namespace cube {

// type: 0 num, 1 bool, 2 enum, 3 palette, 4 string, 5 color
struct ParamSpec {
  const char* key;
  const char* label;
  uint8_t type;
  float minV, maxV, stepV;
  float defNum;          // number/bool default
  const char* defStr;    // enum/palette/string default
  const char* options;   // comma-separated, enum only
  const char* desc;      // one-line hover help shown in the console UI
};

// Shared audio-throb block (cube_throb.h): same three knobs, same keys, on
// every pattern that dips its brightness between beats. Splice into a spec
// array with the pattern's default depth (0 = steady until dialed up).
#define CUBE_THROB_SPECS(defDepth)                                            \
  {"throbDepth", "Audio throb depth", 0, 0.0f, 0.8f, 0.05f, defDepth, "", "", \
   "Brightness dips this far between beats; each beat flashes back to full. " \
   "0 = steady."},                                                            \
  {"throbAttack", "Throb attack (s)", 0, 0.0f, 0.5f, 0.01f, 0.03f, "", "",    \
   "Seconds for a beat to flash up to full brightness. 0 = instant snap."},   \
  {"throbRelease", "Throb release (s)", 0, 0.05f, 2.0f, 0.05f, 0.5f, "", "",  \
   "Seconds to sink back down after a beat. Longer = smoother, less twitchy."},

// Shared single-color ("solo") palette block (cube_palettes.h): splice into
// every pattern that has a "palette" param. Off = the palette spreads across
// the pattern as usual; on = the whole pattern is one palette color at a time,
// drifting through the palette.
#define CUBE_PALETTE_SOLO_SPECS                                               \
  {"paletteSolo", "Single color (drift)", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", "",  \
   "One palette color at a time for the whole pattern, drifting through the "  \
   "palette instead of spreading it across the cube."},                        \
  {"soloSpeed", "Drift speed (passes/s)", 0, 0.005f, 0.5f, 0.005f, 0.05f, "", \
   "",                                                                         \
   "How fast the single color travels through the palette. 0.05 = one pass "   \
   "every 20s. Needs single-color mode on."},

static const ParamSpec kSpecs_wavy_sheet[] = {
    {"axis", "Wave-height axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "x,y,z",
     "Which cube axis the sheet's height runs along."},
    {"baseZ", "Base layer", 0, 0.0f, 9.0f, 1.0f, 4.0f, "", "",
     "Resting height of the sheet, in layers."},
    {"amp", "Wave amplitude", 0, 0.0f, 4.0f, 0.1f, 1.2f, "", "",
     "Wave height in voxels, before any audio boost."},
    {"speed", "Wave speed", 0, 0.0f, 3.0f, 0.05f, 0.6f, "", "",
     "How fast the waves travel across the sheet."},
    {"wavelength", "Wavelength (pixels)", 0, 2.0f, 30.0f, 0.5f, 8.0f, "", "",
     "Distance between wave crests, in voxels."},
    {"thickness", "Sheet thickness", 0, 0.4f, 1.5f, 0.1f, 1.0f, "", "",
     "Vertical thickness of the lit sheet."},
    {"edgeSoft", "Edge softness", 0, 0.0f, 3.0f, 0.05f, 1.0f, "", "",
     "Edge falloff: 0 = hard lit/unlit slab, 1 = linear fade, higher = softer fringe."},
    {"audioDecay", "Audio decay (s)", 0, 0.0f, 2.0f, 0.05f, 0.0f, "", "",
     "Seconds for audio hits to ease back out. 0 = instant/twitchy response."},
    {"levelGain", "Audio level → amp", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", "",
     "How much overall loudness swells the wave height."},
    {"bassGain", "Bass → spike", 0, 0.0f, 6.0f, 0.1f, 2.5f, "", "",
     "Bass adds a rippling spike radiating from the center."},
    {"midGain", "Mid → wave mod", 0, 0.0f, 3.0f, 0.05f, 0.5f, "", "",
     "Mid frequencies modulate the cross-wave's height."},
    {"trebleGain", "Treble → surface noise", 0, 0.0f, 3.0f, 0.05f, 0.6f, "", "",
     "Treble adds fine, fast shimmer across the surface."},
    {"beatGain", "Beat → amp kick", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Beats kick the wave height and the center bump. 0 = beats fully ignored."},
    {"rotateSpeed", "Rotate speed (rev/s)", 0, 0.0f, 0.5f, 0.005f, 0.02f, "", "",
     "Constant spin of the wave field around the sheet's center."},
    {"rotateAudio", "Audio-reactive rotation", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", "",
     "Let the music drive the spin rate."},
    {"rotateLevelGain", "Level → rotate boost", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Loudness speeds the spin (needs audio-reactive rotation on)."},
    {"rotateBeatGain", "Beat → rotate kick", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", "",
     "Beats kick the spin (needs audio-reactive rotation on)."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Color source; \"none\" uses the RGB sliders."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", "",
     "Sheet red channel when palette is \"none\"."},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", "",
     "Sheet green channel when palette is \"none\"."},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Sheet blue channel when palette is \"none\"."},
};
static const ParamSpec kSpecs_rain[] = {
    {"spawnRate", "Spawn rate (drops/s)", 0, 0.0f, 80.0f, 1.0f, 14.0f, "", "",
     "Ambient drops released per second."},
    {"fallSpeed", "Fall speed (z/s)", 0, 1.0f, 30.0f, 0.5f, 9.0f, "", "",
     "Base fall speed, in layers per second."},
    {"speedJitter", "Speed jitter", 0, 0.0f, 1.0f, 0.05f, 0.4f, "", "",
     "Per-drop speed variation, so drops don't fall in lockstep."},
    {"trail", "Trail decay (per frame)", 0, 0.5f, 0.99f, 0.01f, 0.85f, "", "",
     "How much of each drop lingers per frame — higher = longer streaks."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "arctic", "",
     "Drop colors; \"none\" uses the hue slider below."},
    CUBE_PALETTE_SOLO_SPECS
    {"pos", "Palette pos / hue", 0, 0.0f, 1.0f, 0.01f, 0.6f, "", "",
     "Where in the palette (or hue circle) drops sample their color."},
    {"jitter", "Position jitter", 0, 0.0f, 0.5f, 0.01f, 0.06f, "", "",
     "Per-drop color variation around the base position."},
    {"audioBoost", "Audio spawn boost", 0, 0.0f, 4.0f, 0.1f, 2.0f, "", "",
     "Loudness multiplies the ambient spawn rate."},
    {"beatSpawn", "Beat → drop burst", 0, 0.0f, 30.0f, 1.0f, 0.0f, "", "",
     "Extra drops released all at once on each beat."},
    {"speedFrom", "Speed boost source", 2, 0.0f, 0.0f, 0.0f, 0.0f, "none", "none,level,bpm",
     "What drives the fall-speed boost: loudness, or the detected tempo."},
    {"speedGain", "Speed boost gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Strength of the fall-speed boost (with the source above)."},
};
static const ParamSpec kSpecs_rotating_planes[] = {
    {"speed", "Rotation speed", 0, 0.0f, 1.0f, 0.01f, 0.3f, "", "",
     "Base rotation rate of the planes (rev/s)."},
    {"sweepSpeed", "Sweep speed", 0, 0.0f, 2.0f, 0.01f, 0.4f, "", "",
     "How fast the planes slide back and forth through the cube."},
    {"thickness", "Plane thickness", 0, 0.4f, 4.0f, 0.1f, 0.7f, "", "",
     "Plane thickness in voxels, before the audio gain fattens it."},
    {"planes", "Plane count", 0, 1.0f, 4.0f, 1.0f, 1.0f, "", "",
     "Number of planes spinning at once."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Per-plane colors; \"none\" uses the RGB sliders."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 80.0f, "", "",
     "Plane red channel when palette is \"none\"."},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 200.0f, "", "",
     "Plane green channel when palette is \"none\"."},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Plane blue channel when palette is \"none\"."},
    {"audioGain", "Audio thickness gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Loudness fattens the planes."},
    {"levelSpeedGain", "Level → speed", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Loudness speeds the rotation."},
    {"beatSpeedGain", "Beat → speed kick", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", "",
     "Beats kick the rotation."},
    {"levelSweepGain", "Level → sweep", 0, 0.0f, 4.0f, 0.1f, 1.2f, "", "",
     "Loudness speeds the sweep."},
    {"beatSweepGain", "Beat → sweep kick", 0, 0.0f, 6.0f, 0.1f, 1.5f, "", "",
     "Beats kick the sweep."},
};
static const ParamSpec kSpecs_audio_ripple[] = {
    {"speed", "Ripple speed (units/s)", 0, 1.0f, 15.0f, 0.1f, 5.0f, "", "",
     "How fast rings expand outward."},
    {"levelSpeedGain", "Level → ripple speed", 0, 0.0f, 4.0f, 0.1f, 0.0f, "", "",
     "Loudness accelerates ring expansion — loud passages fling rings out faster."},
    {"thickness", "Ring thickness", 0, 0.4f, 4.0f, 0.1f, 1.0f, "", "",
     "Shell thickness of each ring."},
    {"fade", "Fade time (s)", 0, 0.5f, 5.0f, 0.1f, 2.0f, "", "",
     "Seconds each ring lives before fading out."},
    {"autoSpawn", "Auto-spawn rate (Hz)", 0, 0.0f, 4.0f, 0.1f, 0.4f, "", "",
     "Ambient rings per second, independent of the music."},
    {"beatThreshold", "Beat trigger threshold", 0, 0.1f, 1.0f, 0.05f, 0.3f, "", "",
     "Beat envelope level that fires a ring — lower = more sensitive."},
    {"levelTrigger", "Level-rise trigger", 0, 0.0f, 1.0f, 0.02f, 0.2f, "", "",
     "A sudden loudness jump this big also fires a ring; 0 disables."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", "",
     "Ring colors; the band mix (bass vs treble) picks the position."},
    CUBE_PALETTE_SOLO_SPECS
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.85f, "", "",
     "Color saturation when no palette is active."},
};
static const ParamSpec kSpecs_spectrum_discs[] = {
    {"axis", "Stack axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "y", "x,y,z",
     "Axis the frequency discs stack along."},
    {"minRadius", "Minimum radius", 0, 0.0f, 4.0f, 0.1f, 1.0f, "", "",
     "Disc radius at silence."},
    {"gain", "Audio gain", 0, 0.1f, 4.0f, 0.1f, 1.5f, "", "",
     "Input gain into the disc radii."},
    {"gamma", "Response curve", 0, 0.3f, 3.0f, 0.05f, 0.7f, "", "",
     "Shape of the response; below 1 lifts quiet detail."},
    {"edge", "Edge softness", 0, 0.2f, 2.0f, 0.1f, 0.7f, "", "",
     "Softness of each disc's rim."},
    {"attack", "Attack (s)", 0, 0.0f, 0.5f, 0.01f, 0.04f, "", "",
     "Seconds for a disc to rise to a new peak."},
    {"release", "Release (s)", 0, 0.02f, 1.5f, 0.02f, 0.25f, "", "",
     "Seconds for a disc to fall back down."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", "",
     "Disc colors, mapped along the stack."},
    CUBE_PALETTE_SOLO_SPECS
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.85f, "", "",
     "Color saturation when no palette is active."},
    {"floor", "Brightness floor", 0, 0.0f, 1.0f, 0.02f, 0.1f, "", "",
     "Minimum brightness so idle discs stay faintly visible."},
    {"beatBoost", "Beat boost", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", "",
     "Beats brighten the whole stack."},
};
static const ParamSpec kSpecs_bar_eq[] = {
    {"gain", "Audio gain", 0, 0.1f, 4.0f, 0.1f, 1.5f, "", "",
     "Input gain into the bar heights."},
    {"gamma", "Response curve", 0, 0.3f, 3.0f, 0.05f, 0.8f, "", "",
     "Shape of the response; below 1 lifts quiet detail."},
    {"attack", "Attack (s)", 0, 0.0f, 0.5f, 0.01f, 0.03f, "", "",
     "Seconds for a bar to rise to a new peak."},
    {"release", "Release (s)", 0, 0.02f, 1.5f, 0.02f, 0.3f, "", "",
     "Seconds for a bar to fall back down."},
    {"beatBoost", "Beat boost", 0, 0.0f, 1.0f, 0.05f, 0.2f, "", "",
     "Beats add height to every bar."},
    {"baseHeight", "Idle bar height", 0, 0.0f, 3.0f, 0.1f, 0.6f, "", "",
     "Bar height at silence, so the grid still reads."},
    {"origin", "Bar origin", 2, 0.0f, 0.0f, 0.0f, 0.0f, "bottom", "bottom,center",
     "\"bottom\" = classic columns; \"center\" = bars grow outward from the middle, mirrored."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", "",
     "Bar colors; \"none\" = classic green-to-red EQ."},
    CUBE_PALETTE_SOLO_SPECS
    {"colorBy", "Color by", 2, 0.0f, 0.0f, 0.0f, 0.0f, "bar", "bar,height,band",
     "What picks a voxel's color: its bar, its height, or its frequency band."},
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.9f, "", "",
     "Color saturation when no palette is active."},
};
static const ParamSpec kSpecs_spiral[] = {
    {"axis", "Layer axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "z,y,x,cycle",
     "Axis the spiral layers stack along; \"cycle\" rotates through all three."},
    {"speed", "Draw speed (cells/s)", 0, 2.0f, 60.0f, 1.0f, 18.0f, "", "",
     "How fast the spiral draws itself."},
    {"layerDelay", "Layer phase delay (s)", 0, 0.0f, 1.0f, 0.02f, 0.14f, "", "",
     "Time offset between layers — larger = more corkscrew."},
    {"twist", "Quarter-turns across height", 0, 0.0f, 4.0f, 1.0f, 1.0f, "", "",
     "How many quarter-turns the spiral twists across the stack."},
    {"tailDim", "Oldest-cell brightness", 0, 0.1f, 1.0f, 0.05f, 0.45f, "", "",
     "Brightness of the oldest drawn cells relative to the head."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "cyberpunk", "",
     "Spiral colors."},
    CUBE_PALETTE_SOLO_SPECS
    {"colorBy", "Color by", 2, 0.0f, 0.0f, 0.0f, 0.0f, "position", "position,layer",
     "Color along the spiral's path, or one color per layer."},
    {"levelGain", "Level → speed", 0, 0.0f, 4.0f, 0.1f, 1.0f, "", "",
     "Loudness speeds the drawing."},
    CUBE_THROB_SPECS(0.0f)
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.9f, "", "",
     "Color saturation when no palette is active."},
};
static const ParamSpec kSpecs_spin_cube[] = {
    {"size", "Cube size (voxels)", 0, 2.0f, 9.0f, 0.25f, 5.5f, "", "",
     "Edge length of the tumbling wireframe cube."},
    {"speedA", "Tumble speed A (rev/s)", 0, 0.0f, 0.6f, 0.01f, 0.09f, "", "",
     "Rotation rate around the first tumble axis."},
    {"speedB", "Tumble speed B (rev/s)", 0, 0.0f, 0.6f, 0.01f, 0.06f, "", "",
     "Rotation rate around the second tumble axis."},
    {"breathe", "Size breathing", 0, 0.0f, 0.5f, 0.02f, 0.12f, "", "",
     "Slow grow/shrink cycle as a fraction of the size."},
    {"beatKick", "Beat → spin kick", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", "",
     "Beats kick the tumble speed."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Edge colors; \"none\" uses the RGB sliders."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 240.0f, "", "",
     "Edge red channel when palette is \"none\"."},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 240.0f, "", "",
     "Edge green channel when palette is \"none\"."},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Edge blue channel when palette is \"none\"."},
    {"cornerBoost", "Corner brightness", 0, 1.0f, 2.0f, 0.05f, 1.35f, "", "",
     "Extra brightness at the cube's corners."},
};
static const ParamSpec kSpecs_bounce[] = {
    {"radius", "Radius (voxels)", 0, 1.5f, 4.5f, 0.1f, 2.6f, "", "",
     "Radius of the bouncing ball."},
    {"speed", "Bounce speed (voxels/s)", 0, 1.0f, 14.0f, 0.5f, 5.0f, "", "",
     "How fast the ball travels."},
    {"rings", "Latitude rings", 0, 1.0f, 5.0f, 1.0f, 3.0f, "", "",
     "Latitude rings drawn on the ball's shell."},
    {"meridians", "Meridians", 0, 0.0f, 6.0f, 1.0f, 4.0f, "", "",
     "Longitude lines drawn on the ball's shell."},
    {"spinSpeed", "Tumble (rev/s)", 0, 0.0f, 1.0f, 0.02f, 0.15f, "", "",
     "How fast the shell pattern tumbles."},
    {"beatPulse", "Beat → size pulse", 0, 0.0f, 2.0f, 0.05f, 0.5f, "", "",
     "Beats puff the ball up briefly."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "ocean", "",
     "Shell colors; \"none\" uses the RGB sliders."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 120.0f, "", "",
     "Shell red channel when palette is \"none\"."},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", "",
     "Shell green channel when palette is \"none\"."},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Shell blue channel when palette is \"none\"."},
};
static const ParamSpec kSpecs_orbit[] = {
    {"count", "Particle count (quiet)", 0, 1.0f, 8.0f, 1.0f, 4.0f, "", "",
     "Orbiting particles when the music isn't adding any."},
    {"countFrom", "Extra orbs from", 2, 0.0f, 0.0f, 0.0f, 0.0f, "none",
     "none,level,beat,bpm",
     "What spawns extra orbs: loudness, each beat, or the detected tempo. "
     "They fade in and out rather than popping."},
    {"countMax", "Particle count (loud)", 0, 1.0f, 8.0f, 1.0f, 8.0f, "", "",
     "Ceiling the extra orbs climb to. Needs a source above."},
    {"countGain", "Extra-orb sensitivity", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "How readily the source reaches the loud count. Higher = fewer quiet moments."},
    {"radius", "Orbit radius (voxels)", 0, 1.5f, 4.5f, 0.1f, 3.4f, "", "",
     "Distance of the orbits from the cube's center."},
    {"speed", "Orbit speed (rev/s)", 0, 0.05f, 2.0f, 0.05f, 0.4f, "", "",
     "How fast the particles circle."},
    {"trail", "Trail length", 0, 0.0f, 14.0f, 1.0f, 10.0f, "", "",
     "Length of each particle's fading trail."},
    {"precess", "Precession (rev/s)", 0, 0.0f, 0.5f, 0.01f, 0.05f, "", "",
     "How fast the orbital planes themselves rotate."},
    {"beatSpeed", "Beat → speed", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Beats kick the orbit speed."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", "",
     "Particle colors."},
    CUBE_PALETTE_SOLO_SPECS
    {"headBright", "Head brightness", 0, 0.3f, 1.0f, 0.05f, 1.0f, "", "",
     "Brightness of each particle's head relative to its trail."},
    {"size", "Particle size (voxels)", 0, 0.5f, 3.0f, 0.1f, 0.5f, "", "",
     "Radius of each orb. 0.5 = a single point; trails taper as they fade."},
    {"beatSize", "Beat → size pulse", 0, 0.0f, 2.0f, 0.1f, 0.5f, "", "",
     "How far beats swell the orbs. 0 = fixed size."},
    CUBE_THROB_SPECS(0.0f)
};
static const ParamSpec kSpecs_scan[] = {
    {"axis", "Sweep axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "z,y,x",
     "Axis the bright slice sweeps along."},
    {"speed", "Sweep speed (slices/s)", 0, 2.0f, 40.0f, 1.0f, 11.0f, "", "",
     "How fast the slice sweeps."},
    {"fade", "Slice fade time (s)", 0, 0.1f, 2.0f, 0.05f, 0.55f, "", "",
     "Seconds a slice takes to fade after the sweep passes."},
    {"gridOnly", "Grid lines only (sparser)", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", "",
     "Light only grid lines instead of the full slice."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "arctic", "",
     "Slice colors."},
    CUBE_PALETTE_SOLO_SPECS
    {"colorBySlice", "Color follows slice", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", "",
     "Color shifts with the slice position instead of staying fixed."},
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.85f, "", "",
     "Color saturation when no palette is active."},
    {"levelGain", "Level → speed", 0, 0.0f, 4.0f, 0.1f, 1.0f, "", "",
     "Loudness speeds the sweep."},
};
static const ParamSpec kSpecs_cloud[] = {
    {"threshold", "Density threshold", 0, -0.4f, 0.6f, 0.02f, 0.1f, "", "",
     "Noise level that counts as cloud — higher = sparser, lumpier."},
    {"scale", "Cloud scale", 0, 0.1f, 1.2f, 0.02f, 0.5f, "", "",
     "Spatial size of the lumps; lower = bigger blobs."},
    {"driftSpeed", "Drift speed", 0, 0.0f, 1.0f, 0.02f, 0.25f, "", "",
     "How fast the cloud shape morphs and drifts."},
    {"edge", "Edge softness", 0, 0.05f, 0.6f, 0.01f, 0.18f, "", "",
     "Softness of the cloud's boundary."},
    {"hang", "Ceiling bias", 0, 0.0f, 1.0f, 0.05f, 0.6f, "", "",
     "Pulls the mass up to hang from the cube's ceiling, clear air below."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Cloud colors; \"none\" uses the RGB sliders with storm shading."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", "",
     "Cloud red channel when palette is \"none\"."},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 235.0f, "", "",
     "Cloud green channel when palette is \"none\"."},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Cloud blue channel when palette is \"none\"."},
    {"topTint", "Underside shading", 0, 0.0f, 1.0f, 0.05f, 0.5f, "", "",
     "Darkens the cloud's underside like a storm front."},
    {"spinKick", "Beat spin kick (rad/s)", 0, 0.0f, 6.0f, 0.1f, 0.8f, "", "",
     "Beats give the cloud a spin impulse."},
    {"spinDamp", "Spin damping", 0, 0.2f, 6.0f, 0.1f, 1.2f, "", "",
     "How quickly the spin settles back down."},
    {"beatThreshold", "Beat trigger threshold", 0, 0.1f, 1.0f, 0.05f, 0.5f, "", "",
     "Beat envelope level that triggers spin kicks and bolts."},
    {"boltOnBeat", "Lightning on beat", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", "",
     "Beats hurl a lightning bolt from the cloud's underside."},
    {"boltRate", "Idle bolts (Hz)", 0, 0.0f, 2.0f, 0.05f, 0.15f, "", "",
     "Ambient strikes per second when there's no music; 0 = only on beats."},
    {"boltFade", "Bolt fade (s)", 0, 0.05f, 1.0f, 0.05f, 0.3f, "", "",
     "How long each strike takes to fade."},
    {"flash", "Bolt lights the cloud", 0, 0.0f, 1.0f, 0.05f, 0.35f, "", "",
     "How strongly a strike lights the cloud from inside."},
};
static const ParamSpec kSpecs_comet[] = {
    {"speed", "Base speed (voxels/s)", 0, 1.0f, 30.0f, 0.5f, 8.0f, "", "",
     "How fast the comet flies."},
    {"tailLife", "Tail life (s)", 0, 0.1f, 5.0f, 0.05f, 1.4f, "", "",
     "Seconds the tail takes to fade."},
    {"tailGamma", "Tail fall-off curve", 0, 0.3f, 4.0f, 0.1f, 1.6f, "", "",
     "Tail fade shape; higher = tighter, brighter core."},
    {"r", "Head R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Head red channel."},
    {"g", "Head G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Head green channel."},
    {"b", "Head B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Head blue channel."},
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Tail colors; \"none\" fades the head color."},
    CUBE_PALETTE_SOLO_SPECS
    {"beatGain", "Beat → speed boost", 0, 0.0f, 8.0f, 0.1f, 2.5f, "", "",
     "Beats boost the comet's speed."},
    {"beatThreshold", "Beat trigger threshold", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", "",
     "Beat envelope level that counts as a hit — lower = more sensitive."},
};
static const ParamSpec kSpecs_text_3d[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "planes", "planes,ring,stack",
     "planes = scrolling slices; ring = same char on all 4 faces; stack = one char through the cube."},
    {"text", "Text", 4, 0.0f, 0.0f, 0.0f, 0.0f, "HELLO 123 ", "",
     "The message. A trailing space gives words breathing room."},
    {"axis", "Axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "x,y,z",
     "Axis the characters travel or stack along."},
    {"reverse", "Reverse axis direction", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", "",
     "Flip the travel/stack direction."},
    {"speed", "Base speed", 0, 0.0f, 20.0f, 0.1f, 3.0f, "", "",
     "Scroll/cycle speed before audio boosts."},
    {"charSpacing", "Char spacing (voxels)", 0, 1.0f, 30.0f, 1.0f, 4.0f, "", "",
     "Planes mode: distance between characters."},
    {"stackGap", "Stack: between words", 2, 0.0f, 0.0f, 0.0f, 0.0f, "blank", "blank,dot,dim",
     "What shows during spaces: full blackout, a dot placeholder, or the next letter dimmed."},
    {"levelGain", "Audio level → speed", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", "",
     "Loudness speeds the text."},
    {"beatGain", "Beat → speed kick", 0, 0.0f, 6.0f, 0.1f, 1.5f, "", "",
     "Beats kick the text speed."},
    CUBE_THROB_SPECS(0.0f)
    {"highlightLead", "Planes: highlight lead", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", "",
     "Planes mode: the leading character stays bright, the rest dim."},
    {"trailDim", "Planes: trail brightness", 0, 0.0f, 1.0f, 0.05f, 0.8f, "", "",
     "Planes mode: brightness of non-lead characters."},
    {"leadAt", "Planes: lead at", 2, 0.0f, 0.0f, 0.0f, 0.0f, "front", "front,back",
     "Planes mode: which end of the motion counts as the lead."},
    {"clipL", "Ring: clip left cols", 0, 0.0f, 4.0f, 1.0f, 0.0f, "", "",
     "Ring mode: glyph columns clipped on the left so faces don't overlap."},
    {"clipR", "Ring: clip right cols", 0, 0.0f, 4.0f, 1.0f, 1.0f, "", "",
     "Ring mode: glyph columns clipped on the right so faces don't overlap."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Character colors; \"none\" uses the RGB sliders."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Text red channel when palette is \"none\"."},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 240.0f, "", "",
     "Text green channel when palette is \"none\"."},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 200.0f, "", "",
     "Text blue channel when palette is \"none\"."},
};
static const ParamSpec kSpecs_image_3d[] = {
    {"axis", "Vertical axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "x,y,z",
     "Axis that counts as \"up\" for the art and the background fade."},
    {"level", "Art brightness", 0, 0.0f, 1.0f, 0.05f, 1.0f, "", "",
     "Brightness of the image on the faces."},
    {"bgLevel", "Background glow", 0, 0.0f, 0.6f, 0.02f, 0.12f, "", "",
     "Interior glow in the image's average color. 0 = faces only."},
    {"bgFade", "Background fade", 0, 0.0f, 1.0f, 0.05f, 0.7f, "", "",
     "How much the glow fades toward the top. 0 = even fill."},
    CUBE_THROB_SPECS(0.3f)
    {"imgThrob", "Throb on art", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", "",
     "Fraction of the throb that reaches the art itself; the glow always "
     "takes the full throb."},
};
static const ParamSpec kSpecs_tv_static[] = {
    {"spawnRate", "Ambient glitches/s", 0, 0.0f, 30.0f, 0.5f, 3.0f, "", "",
     "Random glitch pixels per second, music or not."},
    {"beatSpawn", "Beat → glitch burst", 0, 0.0f, 20.0f, 1.0f, 5.0f, "", "",
     "Extra glitches fired on each beat."},
    {"lifeSec", "Glitch life (s)", 0, 0.05f, 1.5f, 0.05f, 0.3f, "", "",
     "How long each glitch flash lasts."},
    {"armLen", "Cross arm length", 0, 0.0f, 4.0f, 0.25f, 1.0f, "", "",
     "Base length of the R/G/B aberration arms, in voxels."},
    {"beatArm", "Beat → arm stretch", 0, 0.0f, 4.0f, 0.25f, 2.0f, "", "",
     "Beats stretch the arms this much further out."},
    {"aberration", "Aberration brightness", 0, 0.0f, 1.0f, 0.05f, 0.8f, "", "",
     "Brightness of the colored arms relative to the center flash."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Glitch colors (each picks a random position); \"none\" = classic white "
     "flash with pure R/G/B arms."},
    CUBE_PALETTE_SOLO_SPECS
    {"gray", "Background gray", 0, 0.0f, 80.0f, 1.0f, 0.0f, "", "",
     "0 = pure black; higher = faint gray dead-channel static behind the glitches."},
    {"grayFlicker", "Background flicker", 0, 0.0f, 1.0f, 0.05f, 0.5f, "", "",
     "Per-pixel noise in the gray background."},
};
static const ParamSpec kSpecs_wander[] = {
    {"speed", "Speed (voxels/s)", 0, 0.5f, 30.0f, 0.5f, 6.0f, "", "",
     "How fast the pixel walks."},
    {"speedFrom", "Speed boost source", 2, 0.0f, 0.0f, 0.0f, 0.0f, "none", "none,level,bpm",
     "What drives the speed boost: loudness, or the detected tempo."},
    {"speedGain", "Speed boost gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Strength of the speed boost (with the source above)."},
    {"turnChance", "Turn chance per step", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", "",
     "Odds of turning a corner at each voxel — higher = jitterier path."},
    {"beatTurn", "Turn on beat", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", "",
     "Every beat makes the pixel turn a corner."},
    {"beatAberration", "Beat → RGB misprint", 0, 0.0f, 4.0f, 0.25f, 1.5f, "", "",
     "Beats split the R and B channels this many voxels off to the sides, "
     "like a mis-registered comic print; the fringes snap out on the beat "
     "and pull back in as it fades. 0 = off."},
    {"tail", "Tail length", 0, 0.0f, 24.0f, 1.0f, 4.0f, "", "",
     "Fading trail behind the pixel; 0 = lone dot."},
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Tail colors; \"none\" fades the RGB color below."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Pixel red channel."},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Pixel green channel."},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Pixel blue channel."},
};
static const ParamSpec kSpecs_snake_3d[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "auto", "auto,manual",
     "auto = the solver plays itself; manual = the game pad drives."},
    {"nearMiss", "Auto: near-miss chance", 0, 0.0f, 1.0f, 0.05f, 0.0f, "", "",
     "Auto mode: odds of theatrically swerving past an apple."},
    {"baseSpeed", "Start speed (Hz)", 0, 1.0f, 12.0f, 0.5f, 1.0f, "", "",
     "Moves per second at the start."},
    {"maxSpeed", "Max speed (Hz)", 0, 4.0f, 25.0f, 0.5f, 12.0f, "", "",
     "Moves per second once the snake is long."},
    {"lengthForMax", "Length for max speed", 0, 5.0f, 60.0f, 1.0f, 25.0f, "", "",
     "Score at which the speed ramp tops out."},
    {"maxLen", "Max tail length (0 = off)", 0, 0.0f, 100.0f, 1.0f, 0.0f, "", "",
     "Cap the snake's length — it keeps scoring but stops growing. Keeps long auto games readable."},
    {"autoLevelGain", "Auto: level → speed", 0, 0.0f, 3.0f, 0.1f, 0.0f, "", "",
     "Auto mode only: loudness speeds the snake up. Manual stays fair."},
    CUBE_THROB_SPECS(0.0f)
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", "",
     "Body colors head-to-tail."},
    CUBE_PALETTE_SOLO_SPECS
    {"appleR", "Apple R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Apple red channel."},
    {"appleG", "Apple G", 0, 0.0f, 255.0f, 1.0f, 40.0f, "", "",
     "Apple green channel."},
    {"appleB", "Apple B", 0, 0.0f, 255.0f, 1.0f, 40.0f, "", "",
     "Apple blue channel."},
    {"hintFullScore", "Hint full until score", 0, 0.0f, 20.0f, 1.0f, 3.0f, "", "",
     "Direction hints show at full strength until this score."},
    {"hintGoneScore", "Hint gone by score", 0, 1.0f, 30.0f, 1.0f, 8.0f, "", "",
     "Direction hints fade out entirely by this score."},
    {"hintFlashSec", "Hint flash duration (s)", 0, 0.1f, 1.5f, 0.05f, 0.45f, "", "",
     "How long each button press flashes its wall hint."},
};
static const ParamSpec kSpecs_pacman_3d[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "auto", "auto,manual",
     "auto = Pac-Man plays himself; manual = the game pad drives."},
    {"baseSpeed", "Pac-Man speed (Hz)", 0, 1.0f, 20.0f, 0.5f, 6.0f, "", "",
     "Pac-Man's moves per second."},
    {"ghostSpeed", "Ghost speed (Hz)", 0, 0.5f, 12.0f, 0.25f, 3.0f, "", "",
     "Ghosts' moves per second."},
    {"ghostCount", "Ghost count", 0, 0.0f, 4.0f, 1.0f, 4.0f, "", "",
     "How many ghosts give chase."},
    {"tailLength", "Tail length", 0, 0.0f, 60.0f, 1.0f, 14.0f, "", "",
     "Length of Pac-Man's rainbow wake."},
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "rainbow", "",
     "Wake colors."},
    CUBE_PALETTE_SOLO_SPECS
    {"pelletBrightness", "Pellet brightness", 0, 0.0f, 1.0f, 0.01f, 0.06f, "", "",
     "Brightness of the uneaten pellet field."},
    {"pelletColor", "Pellet hue (0=warm, 1=cool)", 0, 0.0f, 1.0f, 0.01f, 0.13f, "", "",
     "Tint of the pellet field."},
    {"beatSpeedGain", "Beat → speed kick", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Beats kick everyone's speed."},
    {"beatSparkle", "Beat → pellet sparkle", 0, 0.0f, 1.0f, 0.05f, 0.6f, "", "",
     "Beats make the pellet field sparkle."},
    {"levelGain", "Level → ambient", 0, 0.0f, 2.0f, 0.05f, 0.5f, "", "",
     "Loudness lifts the ambient glow."},
    {"wakaHz", "Waka-waka rate (Hz)", 0, 0.0f, 12.0f, 0.5f, 5.0f, "", "",
     "Mouth-chomp flicker rate."},
    {"ghostFearOnBeat", "Ghosts flicker on beat", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", "",
     "Beats make the ghosts flicker blue like a power pellet."},
};
static const ParamSpec kSpecs_fireworks[] = {
    {"palette", "Burst palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "cyberpunk", "",
     "Colors the bursts draw from."},
    CUBE_PALETTE_SOLO_SPECS
    {"launchInterval", "Seconds between rockets", 0, 0.3f, 6.0f, 0.1f, 1.6f, "", "",
     "Timer-based launch cadence."},
    {"flightTime", "Rocket flight (s)", 0, 0.2f, 2.0f, 0.05f, 0.7f, "", "",
     "Nominal flight before a rocket reaches its target."},
    {"particles", "Particles per burst", 0, 6.0f, 200.0f, 1.0f, 70.0f, "", "",
     "Base particle count per explosion."},
    {"burstSpeed", "Burst speed (cells/s)", 0, 1.0f, 12.0f, 0.25f, 5.5f, "", "",
     "How fast burst particles fly outward."},
    {"gravity", "Gravity (z- per s)", 0, 0.0f, 8.0f, 0.1f, 2.5f, "", "",
     "Downward pull on burst particles."},
    {"drag", "Drag (per s)", 0, 0.0f, 4.0f, 0.05f, 1.4f, "", "",
     "Air resistance slowing burst particles."},
    {"lifetime", "Particle life (s)", 0, 0.3f, 4.0f, 0.1f, 1.4f, "", "",
     "How long burst particles glow."},
    {"rocketBrightness", "Rocket brightness", 0, 0.0f, 1.0f, 0.05f, 0.95f, "", "",
     "Brightness of the rocket and its trail."},
    {"burstOn", "Burst trigger", 2, 0.0f, 0.0f, 0.0f, 0.0f, "timer", "timer,beat",
     "timer = pop at end of flight; beat = pop ONLY when the music hits — quiet rockets fly on through."},
    {"popLevelRise", "Beat mode: level-rise pop", 0, 0.0f, 1.0f, 0.05f, 0.3f, "", "",
     "In beat mode, a loudness surge this big also pops rockets; 0 disables."},
    {"beatLaunch", "Beat launches a rocket", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", "",
     "Beats fire an extra rocket."},
    {"levelBoost", "Level → particle boost", 0, 0.0f, 2.0f, 0.05f, 0.6f, "", "",
     "Loudness adds particles to each burst."},
};
static const ParamSpec kSpecs_solid[] = {
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 64.0f, "", "",
     "Red channel of the whole cube."},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 64.0f, "", "",
     "Green channel of the whole cube."},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 64.0f, "", "",
     "Blue channel of the whole cube."},
};
static const ParamSpec kSpecs_index_walk[] = {
    {"speed", "LEDs/sec", 0, 1.0f, 200.0f, 1.0f, 25.0f, "", "",
     "How many LEDs the walker advances per second, in wire order."},
    {"tail", "Tail length", 0, 0.0f, 50.0f, 1.0f, 6.0f, "", "",
     "Fading tail behind the walker."},
};
static const ParamSpec kSpecs_lit_pixel[] = {
    {"ledIdx", "LED index", 0, 0.0f, 999.0f, 1.0f, 0.0f, "", "",
     "Which single LED (wire order) to light."},
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Red channel."},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Green channel."},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Blue channel."},
};
static const ParamSpec kSpecs_build_map[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "axis", "axis,strand",
     "axis = light the calibrated cube faces; strand = mark raw wire strands."},
    {"axis", "Axis (axis mode)", 2, 0.0f, 0.0f, 0.0f, 0.0f, "x", "x,y,z",
     "Which axis's two faces to light in axis mode."},
    {"period", "Strand length (strand mode)", 0, 2.0f, 100.0f, 1.0f, 10.0f, "", "",
     "LEDs per strand when marking by wire index."},
    {"showCenter", "Mark the two middle", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", "",
     "Also mark each strand's two middle LEDs."},
    {"endR", "End R", 0, 0.0f, 255.0f, 1.0f, 0.0f, "", "",
     "End-marker red channel."},
    {"endG", "End G", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "End-marker green channel."},
    {"endB", "End B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "End-marker blue channel."},
    {"ctrR", "Middle R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Middle-marker red channel."},
    {"ctrG", "Middle G", 0, 0.0f, 255.0f, 1.0f, 90.0f, "", "",
     "Middle-marker green channel."},
    {"ctrB", "Middle B", 0, 0.0f, 255.0f, 1.0f, 0.0f, "", "",
     "Middle-marker blue channel."},
};

static const ParamSpec kSpecs_rail_grind[] = {
    {"axis", "Travel axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "y", "y,x,z,spin",
     "Cube axis the ride runs along; \"spin\" slowly wheels the whole view "
     "around the vertical, like the camera panning as you ride."},
    {"spinSpeed", "Spin speed (deg/s)", 0, 1.0f, 45.0f, 1.0f, 8.0f, "", "",
     "How fast the view rotates in \"spin\" mode."},
    {"speed", "Ride speed (voxels/s)", 0, 1.0f, 30.0f, 0.5f, 8.0f, "", "",
     "How fast you travel along the rail."},
    {"speedFrom", "Speed boost source", 2, 0.0f, 0.0f, 0.0f, 0.0f, "none", "none,level,bpm",
     "What drives the speed boost: loudness, or the detected tempo."},
    {"speedGain", "Speed boost gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Strength of the speed boost (with the source above)."},
    {"curviness", "Curviness (voxels)", 0, 0.0f, 8.0f, 0.25f, 4.0f, "", "",
     "How far the rail swings side to side."},
    {"turnLen", "Turn length (voxels)", 0, 4.0f, 40.0f, 1.0f, 14.0f, "", "",
     "Distance a typical bend takes — short = twisty, long = sweeping."},
    {"vertAmount", "Vertical dips (voxels)", 0, 0.0f, 6.0f, 0.25f, 2.0f, "", "",
     "How much the rail rises and dips. 0 = flat ride."},
    {"camLag", "Camera lag (s)", 0, 0.0f, 1.5f, 0.05f, 0.4f, "", "",
     "How long the camera takes to turn into a curve. More lag = oncoming "
     "turns swing wider across the view before you carve through them."},
    {"jagGain", "Loudness → jaggedness", 0, 0.0f, 3.0f, 0.1f, 1.0f, "", "",
     "Louder music makes the oncoming rail swing harder."},
    {"beatKink", "Beat → swerve (voxels)", 0, 0.0f, 8.0f, 0.25f, 3.0f, "", "",
     "Each beat drops a sudden jog into the rail ahead; watch it ride in, "
     "then lurch through it. 0 = off."},
    {"depthStep", "Lookahead per layer", 0, 0.5f, 4.0f, 0.25f, 1.5f, "", "",
     "Voxels of rail packed into each depth layer — higher shows more track "
     "(and more turns) at once."},
    {"fade", "Depth fade", 0, 0.4f, 1.0f, 0.02f, 0.8f, "", "",
     "Brightness falloff per layer of distance. 1 = far rail as bright as near."},
    {"glow", "Rail glow", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", "",
     "Sideways glow around the rail line. 0 = thin single-voxel line."},
    {"palette", "Rail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", "",
     "Colors along the rail's depth; \"none\" = the RGB color below."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 80.0f, "", "",
     "Rail red channel."},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", "",
     "Rail green channel."},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Rail blue channel."},
};
static const ParamSpec kSpecs_rez_tunnel[] = {
    {"axis", "Tunnel axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "y", "y,x,z,spin",
     "Cube axis the tunnel runs along; \"spin\" slowly wheels the tunnel "
     "mouth around the vertical."},
    {"spinSpeed", "Spin speed (deg/s)", 0, 1.0f, 45.0f, 1.0f, 8.0f, "", "",
     "How fast the view rotates in \"spin\" mode."},
    {"speed", "Fly speed (layers/s)", 0, 1.0f, 30.0f, 0.5f, 6.0f, "", "",
     "Base forward speed through the tunnel."},
    {"speedFrom", "Speed boost source", 2, 0.0f, 0.0f, 0.0f, 0.0f, "level", "none,level,bpm",
     "What drives the speed boost: loudness, or the detected tempo."},
    {"speedGain", "Speed boost gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Strength of the speed boost (with the source above)."},
    {"beatKick", "Beat → speed surge", 0, 0.0f, 4.0f, 0.1f, 1.2f, "", "",
     "Each beat momentarily rushes everything toward you."},
    {"beatSpawn", "Shapes per beat", 0, 0.0f, 4.0f, 1.0f, 1.0f, "", "",
     "Wireframe shapes launched from the horizon on each beat. 0 = sparks only."},
    {"shape", "Shape", 2, 0.0f, 0.0f, 0.0f, 0.0f, "mixed", "ring,square,cross,mixed",
     "What flies at you on beats; mixed picks at random."},
    {"shapeSize", "Shape size (voxels)", 0, 2.0f, 7.0f, 0.25f, 4.5f, "", "",
     "Radius the shapes reach as they arrive at the near face."},
    {"ambientRate", "Ambient sparks/s", 0, 0.0f, 20.0f, 0.5f, 4.0f, "", "",
     "Background starfield streaming past, music or not."},
    {"streak", "Motion streak", 0, 0.0f, 1.0f, 0.05f, 0.4f, "", "",
     "Trailing smear behind each point, hinting at speed. 0 = clean dots."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "rainbow", "",
     "Each shape and spark picks one random color from here; \"none\" = the "
     "RGB color below."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Point red channel (palette \"none\")."},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Point green channel (palette \"none\")."},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Point blue channel (palette \"none\")."},
};

static const ParamSpec kSpecs_double_helix[] = {
    {"axis", "Helix axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "z,y,x",
     "Cube axis the helix climbs along."},
    {"radius", "Radius (voxels)", 0, 1.0f, 4.5f, 0.25f, 3.0f, "", "",
     "How far the strands sit from the axis."},
    {"turns", "Twists over height", 0, 0.25f, 3.0f, 0.25f, 1.2f, "", "",
     "Full rotations each strand makes across the cube."},
    {"speed", "Spin speed (rev/s)", 0, 0.0f, 2.0f, 0.05f, 0.25f, "", "",
     "How fast the whole helix rotates."},
    {"speedFrom", "Speed boost source", 2, 0.0f, 0.0f, 0.0f, 0.0f, "none", "none,level,bpm",
     "What drives the speed boost: loudness, or the detected tempo."},
    {"speedGain", "Speed boost gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", "",
     "Strength of the speed boost (with the source above)."},
    {"beatSpin", "Beat → spin kick", 0, 0.0f, 8.0f, 0.25f, 3.0f, "", "",
     "Each beat shoves the rotation forward; it eases off as the beat fades."},
    {"beatPulse", "Beat → radius puff (voxels)", 0, 0.0f, 3.0f, 0.25f, 0.8f, "", "",
     "Beats swell the helix outward this far."},
    {"strands", "Strands", 0, 1.0f, 3.0f, 1.0f, 2.0f, "", "",
     "How many strands wind around the axis. 2 = classic DNA."},
    {"rungEvery", "Rung every N layers", 0, 0.0f, 5.0f, 1.0f, 3.0f, "", "",
     "Base-pair bridges between the strands (2-strand mode only). 0 = off."},
    {"rungBright", "Rung brightness", 0, 0.0f, 1.0f, 0.05f, 0.4f, "", "",
     "Rung brightness relative to the strands."},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "cycle", "",
     "Colors along the height, each strand offset through it; \"none\" = the "
     "RGB color below for every strand."},
    CUBE_PALETTE_SOLO_SPECS
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 80.0f, "", "",
     "Strand red channel."},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 200.0f, "", "",
     "Strand green channel."},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", "",
     "Strand blue channel."},
};

struct PatternSpecs {
  const char* id;
  const ParamSpec* specs;
  int count;
};

// SPEC_N: sized from the array itself so adding/removing a param can't
// silently desync the count.
#define SPEC_N(arr) ((int)(sizeof(arr) / sizeof((arr)[0])))
static const PatternSpecs kPatternSpecs[] = {
    {"wavy-sheet", kSpecs_wavy_sheet, SPEC_N(kSpecs_wavy_sheet)},
    {"rain", kSpecs_rain, SPEC_N(kSpecs_rain)},
    {"rotating-planes", kSpecs_rotating_planes, SPEC_N(kSpecs_rotating_planes)},
    {"audio-ripple", kSpecs_audio_ripple, SPEC_N(kSpecs_audio_ripple)},
    {"spectrum-discs", kSpecs_spectrum_discs, SPEC_N(kSpecs_spectrum_discs)},
    {"bar-eq", kSpecs_bar_eq, SPEC_N(kSpecs_bar_eq)},
    {"spiral", kSpecs_spiral, SPEC_N(kSpecs_spiral)},
    {"spin-cube", kSpecs_spin_cube, SPEC_N(kSpecs_spin_cube)},
    {"bounce", kSpecs_bounce, SPEC_N(kSpecs_bounce)},
    {"orbit", kSpecs_orbit, SPEC_N(kSpecs_orbit)},
    {"scan", kSpecs_scan, SPEC_N(kSpecs_scan)},
    {"rail-grind", kSpecs_rail_grind, SPEC_N(kSpecs_rail_grind)},
    {"rez-tunnel", kSpecs_rez_tunnel, SPEC_N(kSpecs_rez_tunnel)},
    {"double-helix", kSpecs_double_helix, SPEC_N(kSpecs_double_helix)},
    {"cloud", kSpecs_cloud, SPEC_N(kSpecs_cloud)},
    {"comet", kSpecs_comet, SPEC_N(kSpecs_comet)},
    {"text-3d", kSpecs_text_3d, SPEC_N(kSpecs_text_3d)},
    {"image-3d", kSpecs_image_3d, SPEC_N(kSpecs_image_3d)},
    {"tv-static", kSpecs_tv_static, SPEC_N(kSpecs_tv_static)},
    {"wander", kSpecs_wander, SPEC_N(kSpecs_wander)},
    {"snake-3d", kSpecs_snake_3d, SPEC_N(kSpecs_snake_3d)},
    {"pacman-3d", kSpecs_pacman_3d, SPEC_N(kSpecs_pacman_3d)},
    {"fireworks", kSpecs_fireworks, SPEC_N(kSpecs_fireworks)},
    {"solid", kSpecs_solid, SPEC_N(kSpecs_solid)},
    {"index-walk", kSpecs_index_walk, SPEC_N(kSpecs_index_walk)},
    {"lit-pixel", kSpecs_lit_pixel, SPEC_N(kSpecs_lit_pixel)},
    {"build-map", kSpecs_build_map, SPEC_N(kSpecs_build_map)},
};
#undef SPEC_N
static const int kPatternSpecsCount =
    (int)(sizeof(kPatternSpecs) / sizeof(kPatternSpecs[0]));

inline const PatternSpecs* specsFor(const char* id) {
  for (int i = 0; i < kPatternSpecsCount; i++) {
    const char* a = kPatternSpecs[i].id;
    const char* b = id;
    while (*a && *a == *b) { a++; b++; }
    if (*a == *b) return &kPatternSpecs[i];
  }
  return nullptr;
}

}  // namespace cube
