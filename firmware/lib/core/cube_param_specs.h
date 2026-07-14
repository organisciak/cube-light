#pragma once
// GENERATED from src/shared/patterns/* by scripts/gen-param-specs.mts — do not edit.
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
};

static const ParamSpec kSpecs_wavy_sheet[] = {
    {"axis", "Wave-height axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "x,y,z"},
    {"baseZ", "Base layer", 0, 0.0f, 9.0f, 1.0f, 4.0f, "", ""},
    {"amp", "Wave amplitude", 0, 0.0f, 4.0f, 0.1f, 1.2f, "", ""},
    {"speed", "Wave speed", 0, 0.0f, 3.0f, 0.05f, 0.6f, "", ""},
    {"wavelength", "Wavelength (pixels)", 0, 2.0f, 30.0f, 0.5f, 8.0f, "", ""},
    {"thickness", "Sheet thickness", 0, 0.4f, 4.0f, 0.1f, 1.0f, "", ""},
    {"levelGain", "Audio level → amp", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", ""},
    {"bassGain", "Bass → spike", 0, 0.0f, 6.0f, 0.1f, 2.5f, "", ""},
    {"midGain", "Mid → wave mod", 0, 0.0f, 3.0f, 0.05f, 0.5f, "", ""},
    {"trebleGain", "Treble → surface noise", 0, 0.0f, 3.0f, 0.05f, 0.6f, "", ""},
    {"beatGain", "Beat → amp kick", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", ""},
    {"rotateSpeed", "Rotate speed (rev/s)", 0, 0.0f, 0.5f, 0.005f, 0.02f, "", ""},
    {"rotateAudio", "Audio-reactive rotation", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", ""},
    {"rotateLevelGain", "Level → rotate boost", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", ""},
    {"rotateBeatGain", "Beat → rotate kick", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", ""},
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", ""},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", ""},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
};
static const ParamSpec kSpecs_rain[] = {
    {"spawnRate", "Spawn rate (drops/s)", 0, 0.0f, 80.0f, 1.0f, 14.0f, "", ""},
    {"fallSpeed", "Fall speed (z/s)", 0, 1.0f, 30.0f, 0.5f, 9.0f, "", ""},
    {"speedJitter", "Speed jitter", 0, 0.0f, 1.0f, 0.05f, 0.4f, "", ""},
    {"trail", "Trail decay (per frame)", 0, 0.5f, 0.99f, 0.01f, 0.85f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "arctic", ""},
    {"pos", "Palette pos / hue", 0, 0.0f, 1.0f, 0.01f, 0.6f, "", ""},
    {"jitter", "Position jitter", 0, 0.0f, 0.5f, 0.01f, 0.06f, "", ""},
    {"audioBoost", "Audio spawn boost", 0, 0.0f, 4.0f, 0.1f, 2.0f, "", ""},
};
static const ParamSpec kSpecs_rotating_planes[] = {
    {"speed", "Rotation speed", 0, 0.0f, 2.0f, 0.01f, 0.3f, "", ""},
    {"sweepSpeed", "Sweep speed", 0, 0.0f, 2.0f, 0.01f, 0.4f, "", ""},
    {"thickness", "Plane thickness", 0, 0.4f, 4.0f, 0.1f, 0.7f, "", ""},
    {"planes", "Plane count", 0, 1.0f, 4.0f, 1.0f, 1.0f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", ""},
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 80.0f, "", ""},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 200.0f, "", ""},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"audioGain", "Audio thickness gain", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", ""},
    {"levelSpeedGain", "Level → speed", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", ""},
    {"beatSpeedGain", "Beat → speed kick", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", ""},
    {"levelSweepGain", "Level → sweep", 0, 0.0f, 4.0f, 0.1f, 1.2f, "", ""},
    {"beatSweepGain", "Beat → sweep kick", 0, 0.0f, 6.0f, 0.1f, 1.5f, "", ""},
};
static const ParamSpec kSpecs_audio_ripple[] = {
    {"speed", "Ripple speed (units/s)", 0, 1.0f, 15.0f, 0.1f, 5.0f, "", ""},
    {"thickness", "Ring thickness", 0, 0.4f, 4.0f, 0.1f, 1.0f, "", ""},
    {"fade", "Fade time (s)", 0, 0.5f, 5.0f, 0.1f, 2.0f, "", ""},
    {"autoSpawn", "Auto-spawn rate (Hz)", 0, 0.0f, 4.0f, 0.1f, 0.4f, "", ""},
    {"beatThreshold", "Beat trigger threshold", 0, 0.1f, 1.0f, 0.05f, 0.3f, "", ""},
    {"levelTrigger", "Level-rise trigger", 0, 0.0f, 1.0f, 0.02f, 0.2f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", ""},
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.85f, "", ""},
};
static const ParamSpec kSpecs_spectrum_discs[] = {
    {"axis", "Stack axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "y", "x,y,z"},
    {"minRadius", "Minimum radius", 0, 0.0f, 4.0f, 0.1f, 1.0f, "", ""},
    {"gain", "Audio gain", 0, 0.1f, 4.0f, 0.1f, 1.5f, "", ""},
    {"gamma", "Response curve", 0, 0.3f, 3.0f, 0.05f, 0.7f, "", ""},
    {"edge", "Edge softness", 0, 0.2f, 2.0f, 0.1f, 0.7f, "", ""},
    {"attack", "Attack (s)", 0, 0.0f, 0.5f, 0.01f, 0.04f, "", ""},
    {"release", "Release (s)", 0, 0.02f, 1.5f, 0.02f, 0.25f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", ""},
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.85f, "", ""},
    {"floor", "Brightness floor", 0, 0.0f, 1.0f, 0.02f, 0.1f, "", ""},
    {"beatBoost", "Beat boost", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", ""},
};
static const ParamSpec kSpecs_bar_eq[] = {
    {"gain", "Audio gain", 0, 0.1f, 4.0f, 0.1f, 1.5f, "", ""},
    {"gamma", "Response curve", 0, 0.3f, 3.0f, 0.05f, 0.8f, "", ""},
    {"attack", "Attack (s)", 0, 0.0f, 0.5f, 0.01f, 0.03f, "", ""},
    {"release", "Release (s)", 0, 0.02f, 1.5f, 0.02f, 0.3f, "", ""},
    {"beatBoost", "Beat boost", 0, 0.0f, 1.0f, 0.05f, 0.2f, "", ""},
    {"baseHeight", "Idle bar height", 0, 0.0f, 3.0f, 0.1f, 0.6f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", ""},
    {"colorBy", "Color by", 2, 0.0f, 0.0f, 0.0f, 0.0f, "bar", "bar,height,band"},
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.9f, "", ""},
};
static const ParamSpec kSpecs_spiral[] = {
    {"axis", "Layer axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "z,y,x"},
    {"speed", "Draw speed (cells/s)", 0, 2.0f, 60.0f, 1.0f, 18.0f, "", ""},
    {"layerDelay", "Layer phase delay (s)", 0, 0.0f, 1.0f, 0.02f, 0.14f, "", ""},
    {"twist", "Quarter-turns across height", 0, 0.0f, 4.0f, 1.0f, 1.0f, "", ""},
    {"tailDim", "Oldest-cell brightness", 0, 0.1f, 1.0f, 0.05f, 0.45f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "cyberpunk", ""},
    {"colorBy", "Color by", 2, 0.0f, 0.0f, 0.0f, 0.0f, "position", "position,layer"},
    {"levelGain", "Level → speed", 0, 0.0f, 4.0f, 0.1f, 1.0f, "", ""},
    {"sat", "Saturation (HSV mode)", 0, 0.0f, 1.0f, 0.05f, 0.9f, "", ""},
};
static const ParamSpec kSpecs_spin_cube[] = {
    {"size", "Cube size (voxels)", 0, 2.0f, 9.0f, 0.25f, 5.5f, "", ""},
    {"speedA", "Tumble speed A (rev/s)", 0, 0.0f, 0.6f, 0.01f, 0.09f, "", ""},
    {"speedB", "Tumble speed B (rev/s)", 0, 0.0f, 0.6f, 0.01f, 0.06f, "", ""},
    {"breathe", "Size breathing", 0, 0.0f, 0.5f, 0.02f, 0.12f, "", ""},
    {"beatKick", "Beat → spin kick", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", ""},
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 240.0f, "", ""},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 240.0f, "", ""},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"cornerBoost", "Corner brightness", 0, 1.0f, 2.0f, 0.05f, 1.35f, "", ""},
};
static const ParamSpec kSpecs_cloud[] = {
    {"threshold", "Density threshold", 0, -0.4f, 0.6f, 0.02f, 0.0f, "", ""},
    {"scale", "Cloud scale", 0, 0.1f, 1.2f, 0.02f, 0.5f, "", ""},
    {"driftSpeed", "Drift speed", 0, 0.0f, 1.0f, 0.02f, 0.25f, "", ""},
    {"edge", "Edge softness", 0, 0.05f, 0.6f, 0.01f, 0.18f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", ""},
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 220.0f, "", ""},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 235.0f, "", ""},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"topTint", "Top→bottom tint shift", 0, 0.0f, 1.0f, 0.05f, 0.4f, "", ""},
    {"spinKick", "Beat spin kick (rad/s)", 0, 0.0f, 6.0f, 0.1f, 1.4f, "", ""},
    {"spinDamp", "Spin damping", 0, 0.2f, 6.0f, 0.1f, 1.2f, "", ""},
    {"beatThreshold", "Beat trigger threshold", 0, 0.1f, 1.0f, 0.05f, 0.5f, "", ""},
};
static const ParamSpec kSpecs_comet[] = {
    {"speed", "Base speed (voxels/s)", 0, 1.0f, 30.0f, 0.5f, 8.0f, "", ""},
    {"tailLife", "Tail life (s)", 0, 0.1f, 5.0f, 0.05f, 1.4f, "", ""},
    {"tailGamma", "Tail fall-off curve", 0, 0.3f, 4.0f, 0.1f, 1.6f, "", ""},
    {"r", "Head R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"g", "Head G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"b", "Head B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", ""},
    {"beatGain", "Beat → speed boost", 0, 0.0f, 8.0f, 0.1f, 2.5f, "", ""},
    {"beatThreshold", "Beat trigger threshold", 0, 0.0f, 1.0f, 0.05f, 0.25f, "", ""},
};
static const ParamSpec kSpecs_text_3d[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "planes", "planes,ring,stack"},
    {"text", "Text", 4, 0.0f, 0.0f, 0.0f, 0.0f, "HELLO 123 ", ""},
    {"axis", "Axis", 2, 0.0f, 0.0f, 0.0f, 0.0f, "z", "x,y,z"},
    {"reverse", "Reverse axis direction", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", ""},
    {"speed", "Base speed", 0, 0.0f, 20.0f, 0.1f, 3.0f, "", ""},
    {"charSpacing", "Char spacing (voxels)", 0, 1.0f, 30.0f, 1.0f, 4.0f, "", ""},
    {"levelGain", "Audio level → speed", 0, 0.0f, 6.0f, 0.1f, 2.0f, "", ""},
    {"beatGain", "Beat → speed kick", 0, 0.0f, 6.0f, 0.1f, 1.5f, "", ""},
    {"highlightLead", "Planes: highlight lead", 1, 0.0f, 0.0f, 0.0f, 0.0f, "", ""},
    {"trailDim", "Planes: trail brightness", 0, 0.0f, 1.0f, 0.05f, 0.8f, "", ""},
    {"leadAt", "Planes: lead at", 2, 0.0f, 0.0f, 0.0f, 0.0f, "front", "front,back"},
    {"clipL", "Ring: clip left cols", 0, 0.0f, 4.0f, 1.0f, 0.0f, "", ""},
    {"clipR", "Ring: clip right cols", 0, 0.0f, 4.0f, 1.0f, 1.0f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "none", ""},
    {"r", "R (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"g", "G (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 240.0f, "", ""},
    {"b", "B (RGB mode)", 0, 0.0f, 255.0f, 1.0f, 200.0f, "", ""},
};
static const ParamSpec kSpecs_fire[] = {
    {"cooling", "Cooling", 0, 0.5f, 4.0f, 0.05f, 1.4f, "", ""},
    {"sparking", "Sparking rate", 0, 0.0f, 1.0f, 0.01f, 0.55f, "", ""},
    {"sparkHeat", "Spark heat", 0, 80.0f, 255.0f, 5.0f, 200.0f, "", ""},
    {"baseLayers", "Spark zone (z layers)", 0, 1.0f, 4.0f, 1.0f, 2.0f, "", ""},
    {"driftRate", "Drift rate", 0, 0.3f, 1.0f, 0.01f, 0.85f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "fire", ""},
    {"audioGain", "Audio fan", 0, 0.0f, 5.0f, 0.1f, 2.0f, "", ""},
};
static const ParamSpec kSpecs_life_3d[] = {
    {"rule", "Rule (S/B)", 2, 0.0f, 0.0f, 0.0f, 0.0f, "5-7/6", "5-7/6,4/5-7,5/4-6,4-5/5,6-8/5-7"},
    {"stepHz", "Steps per second", 0, 1.0f, 30.0f, 0.5f, 6.0f, "", ""},
    {"density", "Seed density", 0, 0.1f, 0.6f, 0.01f, 0.32f, "", ""},
    {"palette", "Palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", ""},
    {"fadeAge", "Cell fade-in age", 0, 1.0f, 30.0f, 1.0f, 6.0f, "", ""},
};
static const ParamSpec kSpecs_snake_3d[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "auto", "auto,manual"},
    {"nearMiss", "Auto: near-miss chance", 0, 0.0f, 1.0f, 0.05f, 0.0f, "", ""},
    {"baseSpeed", "Start speed (Hz)", 0, 1.0f, 12.0f, 0.5f, 1.0f, "", ""},
    {"maxSpeed", "Max speed (Hz)", 0, 4.0f, 25.0f, 0.5f, 12.0f, "", ""},
    {"lengthForMax", "Length for max speed", 0, 5.0f, 60.0f, 1.0f, 25.0f, "", ""},
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "spectrum", ""},
    {"appleR", "Apple R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"appleG", "Apple G", 0, 0.0f, 255.0f, 1.0f, 40.0f, "", ""},
    {"appleB", "Apple B", 0, 0.0f, 255.0f, 1.0f, 40.0f, "", ""},
    {"hintFullScore", "Hint full until score", 0, 0.0f, 20.0f, 1.0f, 3.0f, "", ""},
    {"hintGoneScore", "Hint gone by score", 0, 1.0f, 30.0f, 1.0f, 8.0f, "", ""},
    {"hintFlashSec", "Hint flash duration (s)", 0, 0.1f, 1.5f, 0.05f, 0.45f, "", ""},
};
static const ParamSpec kSpecs_pacman_3d[] = {
    {"mode", "Mode", 2, 0.0f, 0.0f, 0.0f, 0.0f, "auto", "auto,manual"},
    {"baseSpeed", "Pac-Man speed (Hz)", 0, 1.0f, 20.0f, 0.5f, 6.0f, "", ""},
    {"ghostSpeed", "Ghost speed (Hz)", 0, 0.5f, 12.0f, 0.25f, 3.0f, "", ""},
    {"ghostCount", "Ghost count", 0, 0.0f, 4.0f, 1.0f, 4.0f, "", ""},
    {"tailLength", "Tail length", 0, 0.0f, 60.0f, 1.0f, 14.0f, "", ""},
    {"palette", "Tail palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "rainbow", ""},
    {"pelletBrightness", "Pellet brightness", 0, 0.0f, 1.0f, 0.01f, 0.06f, "", ""},
    {"pelletColor", "Pellet hue (0=warm, 1=cool)", 0, 0.0f, 1.0f, 0.01f, 0.13f, "", ""},
    {"beatSpeedGain", "Beat → speed kick", 0, 0.0f, 4.0f, 0.1f, 1.5f, "", ""},
    {"beatSparkle", "Beat → pellet sparkle", 0, 0.0f, 1.0f, 0.05f, 0.6f, "", ""},
    {"levelGain", "Level → ambient", 0, 0.0f, 2.0f, 0.05f, 0.5f, "", ""},
    {"wakaHz", "Waka-waka rate (Hz)", 0, 0.0f, 12.0f, 0.5f, 5.0f, "", ""},
    {"ghostFearOnBeat", "Ghosts flicker on beat", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", ""},
};
static const ParamSpec kSpecs_fireworks[] = {
    {"palette", "Burst palette", 3, 0.0f, 0.0f, 0.0f, 0.0f, "cyberpunk", ""},
    {"launchInterval", "Seconds between rockets", 0, 0.3f, 6.0f, 0.1f, 1.6f, "", ""},
    {"flightTime", "Rocket flight (s)", 0, 0.2f, 2.0f, 0.05f, 0.7f, "", ""},
    {"particles", "Particles per burst", 0, 6.0f, 200.0f, 1.0f, 70.0f, "", ""},
    {"burstSpeed", "Burst speed (cells/s)", 0, 1.0f, 12.0f, 0.25f, 5.5f, "", ""},
    {"gravity", "Gravity (z- per s)", 0, 0.0f, 8.0f, 0.1f, 2.5f, "", ""},
    {"drag", "Drag (per s)", 0, 0.0f, 4.0f, 0.05f, 1.4f, "", ""},
    {"lifetime", "Particle life (s)", 0, 0.3f, 4.0f, 0.1f, 1.4f, "", ""},
    {"rocketBrightness", "Rocket brightness", 0, 0.0f, 1.0f, 0.05f, 0.95f, "", ""},
    {"beatLaunch", "Beat launches a rocket", 1, 0.0f, 0.0f, 0.0f, 1.0f, "", ""},
    {"levelBoost", "Level → particle boost", 0, 0.0f, 2.0f, 0.05f, 0.6f, "", ""},
};
static const ParamSpec kSpecs_solid[] = {
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 64.0f, "", ""},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 64.0f, "", ""},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 64.0f, "", ""},
};
static const ParamSpec kSpecs_index_walk[] = {
    {"speed", "LEDs/sec", 0, 1.0f, 200.0f, 1.0f, 25.0f, "", ""},
    {"tail", "Tail length", 0, 0.0f, 50.0f, 1.0f, 6.0f, "", ""},
};
static const ParamSpec kSpecs_lit_pixel[] = {
    {"ledIdx", "LED index", 0, 0.0f, 999.0f, 1.0f, 0.0f, "", ""},
    {"r", "R", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"g", "G", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
    {"b", "B", 0, 0.0f, 255.0f, 1.0f, 255.0f, "", ""},
};

struct PatternSpecs {
  const char* id;
  const ParamSpec* specs;
  int count;
};

static const PatternSpecs kPatternSpecs[] = {
    {"wavy-sheet", kSpecs_wavy_sheet, 19},
    {"rain", kSpecs_rain, 8},
    {"rotating-planes", kSpecs_rotating_planes, 13},
    {"audio-ripple", kSpecs_audio_ripple, 8},
    {"spectrum-discs", kSpecs_spectrum_discs, 11},
    {"bar-eq", kSpecs_bar_eq, 9},
    {"spiral", kSpecs_spiral, 9},
    {"spin-cube", kSpecs_spin_cube, 10},
    {"cloud", kSpecs_cloud, 12},
    {"comet", kSpecs_comet, 9},
    {"text-3d", kSpecs_text_3d, 17},
    {"fire", kSpecs_fire, 7},
    {"life-3d", kSpecs_life_3d, 5},
    {"snake-3d", kSpecs_snake_3d, 12},
    {"pacman-3d", kSpecs_pacman_3d, 13},
    {"fireworks", kSpecs_fireworks, 11},
    {"solid", kSpecs_solid, 3},
    {"index-walk", kSpecs_index_walk, 2},
    {"lit-pixel", kSpecs_lit_pixel, 4},
};
static const int kPatternSpecsCount = 19;

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
